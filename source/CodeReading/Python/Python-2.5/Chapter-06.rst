###############################################################################
Chapter 06 - 最简单的Python 模拟 —— Small Python
###############################################################################

..
    # with overline, for parts
    * with overline, for chapters
    =, for sections
    -, for subsections
    ^, for subsubsections
    ", for paragraphs

.. contents::

*******************************************************************************
6.1 Small Python
*******************************************************************************

在详细考察了 Python 中最常用的几个对象之后， 我们现在完全可以利用这些对象做出一个最\
简单的 Python。 这一章的目的就是模拟出这样一个最简单的 Python， 我们把它称为 \
Small Python。

在 Small Python 中， 我们首先需要实现之前已经分析过的那些对象， 比如 \
``PyInt-Object``， 与 CPython 不同的是， 我们并没有实现像 CPython 那样复杂的机制\
， 作为一个模拟程序， 只实现了简单的功能， 也没有引入对象缓冲池的机制。 这一切都是为\
了简洁而清晰地展示出 Python 运行时的脉络。

在 Small Python 中， 实际上还需要实现 Python 的运行时环境， Python 的运行时环境是\
我们在以后的章节中将要剖析的重点。 在这里只是展示了其核心的思想 —— 利用 \
``PyDictObject`` 对象来维护变量名到变量值的映射。

在 CPython 中， 还有许多其他的主题， 比如 Python 源代码的编译， Python 字节码的生\
成和执行等， 在 Small Python 中， 我们都不会涉及， 因为到目前为止， 我们没有任何能\
力做出如此逼真的模拟。 不过当我们跟随本书完成了 Python 源代码的探索之后， 就完全有能\
力实现一个真正的 Python 了。

在 Small Python 中， 我们仅仅实现了 ``PyIntObject``、 ``PyStringObject`` 及 \
``PyDictObject`` 对象， 仅仅实现了加法运算和输出操作。 同时编译的过程也被简化到了极\
致， 因此我们的 Small Python 只能处理非常受限的表达式。 虽然很简陋， 但从中可以看到 \
Python 的骨架， 同时， 这也是我们深入 Python 虚拟机和运行时的起点。

*******************************************************************************
6.2 对象机制
*******************************************************************************

在 Small Python 中， 对象机制与 CPython 完全相同：

.. topic:: [PyObject]

    .. code-block:: c

        #define PyObject_HEAD \
            int refCount;\
            struct tagPyTypeObject *type

        #define PyObject_HEAD_INIT(typePtr)\
            0, typePtr
        
        typedef struct tagPyObject
        {
            PyObject_HEAD;
        } PyObject;

但是对于类型对象， 我们进行了大规模的删减。最终在类型对象中，只定义了加法操
作，hash 操作以及输出操作：

.. topic:: [PyTypeObject]
    
    .. code-block:: c

        typedef void (*PrintFun)(PyObject* object);
        typedef PyObject* (*AddFun)(PyObject* left, PyObject* right);
        typedef long (*HashFun)(PyObject* object);
        typedef struct tagPyTypeObject
        {
            PyObject_HEAD;
            char* name;
            PrintFun print;
            AddFun add;
            HashFun hash;
        } PyTypeObject;

``PyIntObject`` 的实现与 CPython 几乎是一样的， 不过没有复杂的对象缓冲机制：

.. topic:: [PyIntObject]

    .. code-block:: c

        typedef struct tagPyIntObject
        {
            PyObject_HEAD;
            int value;
        }PyIntObject;
        
        PyObject* PyInt_Create(int value)
        {
            PyIntObject* object = new PyIntObject;
            object->refCount = 1;
            object->type = &PyInt_Type;
            object->value = value;
            return (PyObject*)object;
        }

        static void int_print(PyObject* object)
        {
            PyIntObject* intObject = (PyIntObject*)object;
            printf("%d\n", intObject->value);
        }

        static PyObject* int_add(PyObject* left, PyObject* right)
        {
            PyIntObject* leftInt = (PyIntObject*)left;
            PyIntObject* rightInt = (PyIntObject*)right;
            PyIntObject* result = (PyIntObject*)PyInt_Create(0);
            if(result == NULL)
            {
                printf("We have no enough memory!!");
                exit(1);
            }
            else
            {
                result->value = leftInt->value + rightInt->value;
            }
            return (PyObject*)result;
        }

        static long int_hash(PyObject* object)
        {
            return (long)((PyIntObject*)object)->value;
        }

        PyTypeObject PyInt_Type =
        {
            PyObject_HEAD_INIT(&PyType_Type),
            "int",
            int_print,
            int_add,
            int_hash
        };

Small Python 中的 ``PyStringObject`` 与 CPython 中大不相同， 在 CPython 中， \
``PyStringObject`` 是一个变长对象， 而 Small Python 中只是一个简单的定长对象， 因\
为 Small Python 的定位就是个演示的程序:

.. topic:: [PyStrObject]

    .. code-block:: c

        typedef struct tagPyStrObject
        {
            PyObject_HEAD;
            int length;
            long hashValue;
            char value[50];
        } PyStringObject;

        PyObject* PyStr_Create(const char* value)
        {
            PyStringObject* object = new PyStringObject;
            object->refCount = 1;
            object->type = &PyString_Type;
            object->length = (value == NULL) ? 0 : strlen(value);
            object->hashValue = -1;
            memset(object->value, 0, 50);
            if(value != NULL)
            {
                strcpy(object->value, value);
            }
            return (PyObject*)object;
        }

        static void string_print(PyObject* object)
        {
            PyStringObject* strObject = (PyStringObject*)object;
            printf("%s\n", strObject->value);
        }

        static long string_hash(PyObject* object)
        {
            PyStringObject* strObject = (PyStringObject*)object;
            register int len;
            register unsigned char *p;
            register long x;
            if (strObject->hashValue != -1)
                return strObject->hashValue;
            len = strObject->length;
            p = (unsigned char *)strObject->value;
            x = *p << 7;
            while (--len >= 0)
                x = (1000003*x) ^ *p++;
            x ^= strObject->length;
            if (x == -1)
                x = -2;
            strObject->hashValue = x;
            return x;
        }

        static PyObject* string_add(PyObject* left, PyObject* right)
        {
            PyStringObject* leftStr = (PyStringObject*)left;
            PyStringObject* rightStr = (PyStringObject*)right;
            PyStringObject* result = (PyStringObject*)PyStr_Create(NULL);
            if(result == NULL)
            {
                printf("We have no enough memory!!");
                exit(1);
            }
            else
            {
                strcpy(result->value, leftStr->value);
                strcat(result->value, rightStr->value);
            }
            return (PyObject*)result;
        }

        PyTypeObject PyString_Type =
        {
            PyObject_HEAD_INIT(&PyType_Type),
            "str",
            string_print,
            string_add,
            string_hash
        };
