we tried to implement Slot shifting as a platform independent framework, where porting to any RTOS is made possible by simple filling of platform dependent plugin functions.

The framework design approach is made scalable, i.e. framework is designed with scalable data handling class which can be tuned to either Global/partioned/hybrid selection function.
Algorithmic part is also made portable,i.e., the functions in algorithmic part are very much made disassociated enabling the replacing of any core functionality with other.
