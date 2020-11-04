Python 源码阅读系列 2
---------------------------------

前言
===================

承接上文。

在 Python 的实现中，对某些会频繁执行的代码，都会同时提供函数和宏两种版本，比如上午中的 \
PyInt_AS_LONG ， 与之对应的还有一个函数 PyInt_AsLong 。宏版本的 PyInt_AS_LONG 可以省\
去一次函数调用的开销，但是其牺牲了类被安全，因为其参数 op 完全可以不是一个 PyIntObject \
对象， intobject.c 中的函数版 PyInt_AsLong 则会多方检查类型安全性，但是牺牲了执行效率。

从 PyIntObject 对象的加法操作的实现可以清晰地看到 PyIntObject 是一个 immutable 的对\
象， 因为操作完成后，原来参与操作的任何一个对象都没有发生改变，取而代之的是一个全新的 \
PyIntObject 对象诞生。

如果加法结果溢出，其结果就不是一个 PyIntObject 对象，而是一个 PyLongObject 对象。例如：

.. image:: img/2-1.png 

图 2-1 加法溢出的例中

.. image:: img/2-1-0.png 

图 2-1-0 Python 3.7.7 版本实际结果


