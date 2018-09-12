Implementation of Slotshifting scheduler Algorithm in Linux kernel using Litmus-RT Patch
=========================================================================================

Authors: 
* Gokulvas@gmail.com
* vaulttech@gmail.com

* Implement Slot shifting scheduler as a platform independent framework.
  * **What  is slotshifting?**
    *  Time division based work conserving periodic task scheduling algorithm that tries admit Firm-aperiodics.
If Firm aperiodic is admitted, Then it is guaranteed to be provided with enough slots to complete before deadline.
    * [Algorithmic View of slot shifting]( https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/documentations/SlotShifting.pdf )
    * [original paper](https://www.slideshare.net/slideshow/embed_code/key/PJt8vhtGcHvKQ)
* porting to any RTOS should be a simple process of filling platform dependent plugin functions.
* Port the same into LITUMS-RT.
![picture alt](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/documentations/architecture_Overview.jpeg "Title is optional")

FILES :
-------
* Online implementation: [/kernel/litmus/ss_reservations.c](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/kernel/litmus/ss_reservations.c)
* Data structures: [/kernel/include/litmus/ss_reservations.h](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/kernel/include/litmus/ss_reservations.h)
* Offline computed table of jobs and intervals Data structure: [/kernel/include/litmus/ss_inj_data.h](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/kernel/include/litmus/ss_inj_data.h)

The design approach of slot shifting framework is made SCALABLE.

SCALABILITY:
-----------
* framework is designed with scalable data handling class which can be tuned to either Global/partioned/hybrid selection function. 
* Algorithmic part is made portable,i.e., the functions in algorithmic part are very much made disassociated enabling the replacing of any core functionality with other.

USERSPACE:
-----------
* A generic slot shifting specific table parser.
* A seperate distrubition is built with Busybox.

Highlights:
* Slot shifting is implemented generically independent of platform.
* Decision function maintains own state transition of the tasks.
![picture alt](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/documentations/STATE_CHART.jpeg "Title is optional")

* The algorithm is ported to LITMUS-RT reservation framework.
* A configurable user space slot shifting parser is developed.
* Custom Distribution with busybox is created.
* Doxygen style documentation.

P.S. Look into Documentation folder for much detail explanation.
