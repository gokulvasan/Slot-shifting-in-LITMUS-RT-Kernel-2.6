#include <linux/sched.h>

#include <litmus/litmus.h>
#include <litmus/reservation.h>

void reservation_init(struct reservation *res)
{
	memset(res, sizeof(*res), 0);
	res->state = RESERVATION_INACTIVE;
	INIT_LIST_HEAD(&res->clients);
}

struct task_struct* default_dispatch_client(
	struct reservation *res,
	lt_t *for_at_most)
{
	struct reservation_client *client, *next;
	struct task_struct* tsk;

	BUG_ON(res->state != RESERVATION_ACTIVE);
	*for_at_most = 0;

	list_for_each_entry_safe(client, next, &res->clients, list) {
		tsk = client->dispatch(client);
		if (likely(tsk)) {
			return tsk;
		}
	}
	return NULL;
}

static struct task_struct * task_client_dispatch(struct reservation_client *client)
{
	struct task_client *tc = container_of(client, struct task_client, client);
	return tc->task;
}

void task_client_init(struct task_client *tc, struct task_struct *tsk,
	struct reservation *res)
{
	memset(&tc->client, sizeof(tc->client), 0);
	tc->client.dispatch = task_client_dispatch;
	tc->client.reservation = res;
	tc->task = tsk;
}

static void sup_scheduler_update_at(
	struct sup_reservation_environment* sup_env,
	lt_t when)
{
	if (sup_env->next_scheduler_update > when)
		sup_env->next_scheduler_update = when;
}

static void sup_scheduler_update_after(
	struct sup_reservation_environment* sup_env,
	lt_t timeout)
{
	sup_scheduler_update_at(sup_env, sup_env->env.current_time + timeout);
}

static int _sup_queue_depleted(
	struct sup_reservation_environment* sup_env,
	struct reservation *res)
{
	struct list_head *pos;
	struct reservation *queued;
	int passed_earlier = 0;

	list_for_each(pos, &sup_env->depleted_reservations) {
		queued = list_entry(pos, struct reservation, list);
		if (queued->next_replenishment > res->next_replenishment) {
			list_add(&res->list, pos->prev);
			return passed_earlier;
		} else
			passed_earlier = 1;
	}

	list_add_tail(&res->list, &sup_env->depleted_reservations);

	return passed_earlier;
}

static void sup_queue_depleted(
	struct sup_reservation_environment* sup_env,
	struct reservation *res)
{
	int passed_earlier = _sup_queue_depleted(sup_env, res);

	/* check for updated replenishment time */
	if (!passed_earlier)
		sup_scheduler_update_at(sup_env, res->next_replenishment);
}

static int _sup_queue_active(
	struct sup_reservation_environment* sup_env,
	struct reservation *res)
{
	struct list_head *pos;
	struct reservation *queued;
	int passed_active = 0;

	list_for_each(pos, &sup_env->active_reservations) {
		queued = list_entry(pos, struct reservation, list);
		if (queued->priority > res->priority) {
			list_add(&res->list, pos->prev);
			return passed_active;
		} else if (queued->state == RESERVATION_ACTIVE)
			passed_active = 1;
	}

	list_add_tail(&res->list, &sup_env->active_reservations);
	return passed_active;
}

static void sup_queue_active(
	struct sup_reservation_environment* sup_env,
	struct reservation *res)
{
	int passed_active = _sup_queue_active(sup_env, res);

	/* check for possible preemption */
	if (res->state == RESERVATION_ACTIVE && !passed_active)
		sup_env->next_scheduler_update = SUP_RESCHEDULE_NOW;
	else {
		/* Active means this reservation is draining budget => make sure
		 * the scheduler is called to notice when the reservation budget has been
		 * drained completely. */
		sup_scheduler_update_after(sup_env, res->cur_budget);
	}
}

static void sup_queue_reservation(
	struct sup_reservation_environment* sup_env,
	struct reservation *res)
{
	switch (res->state) {
		case RESERVATION_INACTIVE:
			list_add(&res->list, &sup_env->inactive_reservations);
			break;

		case RESERVATION_DEPLETED:
			sup_queue_depleted(sup_env, res);
			break;

		case RESERVATION_ACTIVE_IDLE:
		case RESERVATION_ACTIVE:
			sup_queue_active(sup_env, res);
			break;
	}
}

void sup_add_new_reservation(
	struct sup_reservation_environment* sup_env,
	struct reservation* new_res)
{
	new_res->env = &sup_env->env;
	sup_queue_reservation(sup_env, new_res);
}

struct reservation* sup_find_by_id(struct sup_reservation_environment* sup_env,
	unsigned int id)
{
	struct reservation *res;

	list_for_each_entry(res, &sup_env->active_reservations, list) {
		if (res->id == id)
			return res;
	}
	list_for_each_entry(res, &sup_env->inactive_reservations, list) {
		if (res->id == id)
			return res;
	}
	list_for_each_entry(res, &sup_env->depleted_reservations, list) {
		if (res->id == id)
			return res;
	}

	return NULL;
}

static void sup_charge_budget(
	struct sup_reservation_environment* sup_env,
	lt_t delta)
{
	struct list_head *pos, *next;
	struct reservation *res;

	int encountered_active = 0;

	list_for_each_safe(pos, next, &sup_env->active_reservations) {
		/* charge all ACTIVE_IDLE up to the first ACTIVE reservation */
		res = list_entry(pos, struct reservation, list);
		if (res->state == RESERVATION_ACTIVE) {
			res->ops->drain_budget(res, delta);
			encountered_active = 1;
		} else {
			BUG_ON(res->state != RESERVATION_ACTIVE_IDLE);
			res->ops->drain_budget(res, delta);
		}
		if (res->state == RESERVATION_ACTIVE ||
			res->state == RESERVATION_ACTIVE_IDLE)
		{
			/* make sure scheduler is invoked when this reservation expires
			 * its remaining budget */
			 TRACE("requesting scheduler update for reservation %u in %llu nanoseconds\n",
				res->id, res->cur_budget);
			 sup_scheduler_update_after(sup_env, res->cur_budget);
		}
		if (encountered_active)
			/* stop at the first ACTIVE reservation */
			break;
	}
	TRACE("finished charging budgets\n");
}

static void sup_replenish_budgets(struct sup_reservation_environment* sup_env)
{
	struct list_head *pos, *next;
	struct reservation *res;

	list_for_each_safe(pos, next, &sup_env->depleted_reservations) {
		res = list_entry(pos, struct reservation, list);
		if (res->next_replenishment <= sup_env->env.current_time) {
			res->ops->replenish(res);
		} else {
			/* list is ordered by increasing depletion times */
			break;
		}
	}
	TRACE("finished replenishing budgets\n");

	/* request a scheduler update at the next replenishment instant */
	res = list_first_entry_or_null(&sup_env->depleted_reservations,
		struct reservation, list);
	if (res)
		sup_scheduler_update_at(sup_env, res->next_replenishment);
}

void sup_update_time(
	struct sup_reservation_environment* sup_env,
	lt_t now)
{
	lt_t delta;

	/* If the time didn't advance, there is nothing to do.
	 * This check makes it safe to call sup_advance_time() potentially
	 * multiple times (e.g., via different code paths. */
	TRACE("(sup_update_time) now: %llu, current_time: %llu\n", now, sup_env->env.current_time);
	if (unlikely(now <= sup_env->env.current_time))
		return;

	delta = now - sup_env->env.current_time;
	sup_env->env.current_time = now;

	/* check if future updates are required */
	if (sup_env->next_scheduler_update <= sup_env->env.current_time)
		sup_env->next_scheduler_update = SUP_NO_SCHEDULER_UPDATE;

	/* deplete budgets by passage of time */
	sup_charge_budget(sup_env, delta);

	/* check if any budgets where replenished */
	sup_replenish_budgets(sup_env);
}

struct task_struct* sup_dispatch(struct sup_reservation_environment* sup_env)
{
	struct reservation *res, *next;
	struct task_struct *tsk = NULL;
	lt_t time_slice;

	list_for_each_entry_safe(res, next, &sup_env->active_reservations, list) {
		if (res->state == RESERVATION_ACTIVE) {
			TRACE("SS_TRACE: Reservation %d will call dispatch\n",
				res->id);
			tsk = res->ops->dispatch_client(res, &time_slice);
			if (likely(tsk)) {
				if (time_slice)
				    sup_scheduler_update_after(sup_env, time_slice);
				sup_scheduler_update_after(sup_env, res->cur_budget);
				return tsk;
			}
		} else {
			TRACE("SS_TRACE: Reservation %d was not active\n",
				res->id);
		}
	}

	return NULL;
}

static void sup_res_change_state(
	struct reservation_environment* env,
	struct reservation *res,
	reservation_state_t new_state)
{
	struct sup_reservation_environment* sup_env;

	sup_env = container_of(env, struct sup_reservation_environment, env);

	TRACE("reservation R%d state %d->%d at %llu\n",
		res->id, res->state, new_state, env->current_time);

	list_del(&res->list);
	/* check if we need to reschedule because we lost an active reservation */
	if (res->state == RESERVATION_ACTIVE && !sup_env->will_schedule)
		sup_env->next_scheduler_update = SUP_RESCHEDULE_NOW;
	res->state = new_state;
	sup_queue_reservation(sup_env, res);
}

void sup_init(struct sup_reservation_environment* sup_env)
{
	memset(sup_env, sizeof(*sup_env), 0);

	INIT_LIST_HEAD(&sup_env->active_reservations);
	INIT_LIST_HEAD(&sup_env->depleted_reservations);
	INIT_LIST_HEAD(&sup_env->inactive_reservations);

	sup_env->env.change_state = sup_res_change_state;

	sup_env->next_scheduler_update = SUP_NO_SCHEDULER_UPDATE;
}
