we tried to implement Slot shifting as a platform independent framework, where porting to any RTOS is made possible by simple filling of platform dependent plugin functions.

The framework design approach is made scalable, i.e. framework is designed with scalable data handling class which can be tuned to either Global/partioned/hybrid selection function.
Algorithmic part is made portable,i.e., the functions in algorithmic part are very much made disassociated enabling the replacing of any core functionality with other.

Highlights:
1. Slot shifting is implemented generically independent of platform.
2. The algorithm is ported to LITMUS-RT reservation framework.
3. A configurable user space slot shifting parser is developed.
4. Custom Distribution is made with busybox.

P.S. Look into Documentation folder for much detail explanation.
