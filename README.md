Implementation of Slotshifting scheduler Algorithm in Linux kernel using Litmus-RT Patch
=========================================================================================

Authors:
--------

* Gokul Vasan: Gokulvas@gmail.com
* John Gamboa: vaulttech@gmail.com

Overview:
---------
* Implementation of Slot shifting scheduler as a platform independent framework.
  * **What  is slotshifting?**
    *  Time division based, work conserving periodic task scheduling algorithm that attempts gaurantee and admit the Firm-aperiodics.
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
* The framework is designed for scalable data handling which can be tuned to be either Global/Partioned/Hybrid selection function. 
* Algorithmic part is made portable,i.e., the functionality of the algorithmic class is disassociated from the platform. This enables poratability to any platform.
* Further, functionalities are very well abstracted from one another enabling replacement of any core functionality with the other.

USERSPACE:
-----------
* A generic slot shifting specific table parser.
* A seperate Linux distrubition is built with Busybox to avoid scheduling noise.

Highlights:
* Slot shifting is implemented generically independent of platform.
* Decision function maintains own state transition of the tasks.
![picture alt](https://github.com/gokulvasan/Slot-shifting-in-LITMUS-RT-Kernel-2.6/blob/master/documentations/STATE_CHART.jpeg "Title is optional")

* The algorithm is ported to LITMUS-RT reservation framework.
* A configurable user space slot shifting parser is developed.
* Custom Distribution with busybox is created.
* Doxygen style documentation.

P.S. Look into Documentation folder for much detail explanation.
