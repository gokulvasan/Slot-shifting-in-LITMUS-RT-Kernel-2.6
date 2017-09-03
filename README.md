Authors: Gokulvas@gmail.com
	 vaulttech@gmail.com

GOAL : * Implement Slot shifting scheduler as a platform independent framework.
       * porting to any RTOS should be a simple process of filling platform dependent plugin functions.
       * Port the same into LITUMS-RT.

FILE : /kernel/litmus/ss_reservations.c
       /kernel/include/litmus/ss_reservations.h
       /kernel/include/litmus/ss_inj_data.h

The design approach of slot shifting framework is made SCALABLE.
SCALABLE: 
	* framework is designed with scalable data handling class which can be tuned to either Global/partioned/hybrid selection function. 
	* Algorithmic part is made portable,i.e., the functions in algorithmic part are very much made disassociated enabling the replacing of any core functionality with other.

USERSPACE: 
	* A generic slot shifting specific table parser.

Highlights:
* Slot shifting is implemented generically independent of platform.
* The algorithm is ported to LITMUS-RT reservation framework.
* A configurable user space slot shifting parser is developed.
* Custom Distribution with busybox is created.
* Doxygen style documentation.

P.S. Look into Documentation folder for much detail explanation.
