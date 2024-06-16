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

在 Python 的解释器工作时， 还有一个非常重要的对象， ``PyDictObject`` 对象。 \
``PyDictObject`` 对象在 Python 运行时会维护变量名和变量值的映射关系， Python 所有\
的动作都是基于这种映射关系的。 在 Small Python 中， 我们基于 C++ 中的 map 来实现 \
``PyDictObject`` 对象。 当然， map 的运行效率比 CPython 中所采用的 hash 技术会慢一\
些， 而且对于散列冲突的情况， map 也没有办法解决， 但是对于我们的 Small Python， \
map 就足够了：

.. topic:: [PyDictObject]

    .. code-block:: c

        typedef struct tagPyDictObject
        {
            PyObject_HEAD;
            map<long, PyObject*> dict;
        } PyDictObject;

        PyObject* PyDict_Create()
        {
            PyDictObject* object = new PyDictObject;
            object->refCount = 1;
            object->type = &PyDict_Type;
            return (PyObject*)object;
        }

        PyObject* PyDict_GetItem(PyObject* target, PyObject* key)
        {
            long keyHashValue = (key->type)->hash(key);
            map<long, PyObject*>& dict = ((PyDictObject*)target)->dict;
            map<long, PyObject*>::iterator it = dict.find(keyHashValue);
            map<long, PyObject*>::iterator end = dict.end();
            if(it == end)
            {
                return NULL;
            }
            return it->second;
        }

        int PyDict_SetItem(PyObject* target, PyObject* key, PyObject* value)
        {
            long keyHashValue = (key->type)->hash(key);
            PyDictObject* dictObject = (PyDictObject*)target;
            (dictObject->dict)[keyHashValue] = value;
            return 0;
        }
        //function for PyDict_Type
        static void dict_print(PyObject* object)
        {
            PyDictObject* dictObject = (PyDictObject*)object;
            printf("{");
            map<long, PyObject*>::iterator it = (dictObject->dict).begin();
            map<long, PyObject*>::iterator end = (dictObject->dict).end();
            for( ; it != end; ++it)
            {
                //print key
                printf("%ld : ", it->first);
                //print value
                PyObject* value = it->second;
                (value->type)->print(value);
                printf(", ");
            }
            printf("}\n");
        }

        PyTypeObject PyDict_Type =
        {
            PyObject_HEAD_INIT(&PyType_Type),
            "dict",
            dict_print,
            0,
            0
        };

Small Python 中的对象机制的所有内容都在上边列出了， 非常简单， 对吧， 这就对了， 要\
的就是这个简单。

*******************************************************************************
6.3 解释过程
*******************************************************************************

Small Python 中的这种解释动作还是被简化到了极致， 它实际上就是简单的字符串查找加 \
``if…else…`` 结构：

.. code-block:: c++

    void ExcuteCommand(string& command)
    {
        string::size_type pos = 0;
        if((pos = command.find("print ")) != string::npos)
        {
            ExcutePrint(command.substr(6));
        }
        else if((pos = command.find(" = ")) != string::npos)
        {
            string target = command.substr(0, pos);
            string source = command.substr(pos+3);
            ExcuteAdd(target, source);
        }
    }

    void ExcuteAdd(string& target, string& source)
    {
        string::size_type pos;
        PyObject* strValue = NULL;
        PyObject* resultValue = NULL;
        if(IsSourceAllDigit(source))
        {
            PyObject* intValue = PyInt_Create(atoi(source.c_str()));
            PyObject* key = PyStr_Create(target.c_str());
            PyDict_SetItem(m_LocalEnvironment, key, intValue);
        }
        else if(source.find("\"") != string::npos)
        {
            strValue = PyStr_Create(source.substr(1, source.size()-2).c_str());
            PyObject* key = PyStr_Create(target.c_str());
            PyDict_SetItem(m_LocalEnvironment, key, strValue);
        }
        else if((pos = source.find("+")) != string::npos)
        {
            PyObject* leftObject = GetObjectBySymbol(source.substr(0, pos));
            PyObject* rightObject =
            GetObjectBySymbol(source.substr(pos+1));
            if(leftObject != NULL && right != NULL 
              && leftObject->type == rightObject->type)
            {
                resultValue = (leftObject->type)->add(leftObject,
                rightObject);
                PyObject* key = PyStr_Create(target.c_str());
                PyDict_SetItem(m_LocalEnvironment, key, resultValue);
            }
            (m_LocalEnvironment->type)->print(m_LocalEnvironment);
        }
    }

通过字符串搜索， 如果命令中出现 "="， 就是一个赋值或加法过程； 如果命令中出现 \
"print"， 就是一个输出过程。 在 ``ExcuteAdd`` 中， 还需要进行进一步地字符串搜索， \
以确定是否需要有一个额外的加法过程。 根据这些解析的结果进行不同的动作， 就是 Small \
Python 中的解释过程。 这个过程在 CPython 中是通过正常的编译过程来实现的， 而且最后\
会得到字节码的编译结果。

在这里需要重点指出的是那个 ``m_LocalEnvironment``， 这是一个 ``PyDictObject`` 对\
象， 其维护着 Small Python 运行过程中， 动态创建的变量的变量名和变量值的映射。 这个\
就是 Small Python 中的执行环境， Small Python 正是靠它来维护运行过程中的所有变量的\
状态。 在 CPython 中， 运行环境实际上也是这样一个机制。 当需要访问变量时， 就从这个 \
``PyDictObject`` 对象中查找变量的值。 这一点在执行输出操作时可以看得很清楚：

.. code-block:: c

    PyObject* GetObjectBySymbol(string& symbol)
    {
        PyObject* key = PyStr_Create(symbol.c_str());
        PyObject* value = PyDict_GetItem(m_LocalEnvironment, key);
        if(value == NULL)
        {
            cout << "[Error] : " << symbol << " is not defined!!" << endl;
            return NULL;
        }
        return value;
    }

    void ExcutePrint(string symbol)
    {
        PyObject* object = GetObjectFromSymbol(symbol);
        if(object != NULL)
        {
            PyTypeObject* type = object->type;
            type->print(object);
        }
    }

在这里， 通过变量名 symbol， 获得了变量值 object。 而在刚才的 ExcueteAdd 中， 我们\
将变量名和变量值建立了联系， 并存放到 ``m_LocalEnvironment`` 中。 这种一进一出的机\
制正是 CPython 执行时的关键， 在以后对 Python 字节码解释器的详细剖析中， 我们将真实\
而具体地看到这种机制。

*******************************************************************************
6.4 交互式环境
*******************************************************************************

