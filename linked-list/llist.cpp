#include <stdexcept>
#include <cstddef>

template <typename T>
class LinkedList {
   private:

    struct Node {
       private:
        T _value;
        Node* _next;
        Node* _prev;

       public:
        // 构造函数
        Node() : _value(), _next(nullptr), _prev(nullptr) {}
        Node(T const& val) : _value(val), _next(nullptr), _prev(nullptr) {}

        // 取值赋值函数
        T const& val() const;
        T& val();
        Node* const& next() const;
        Node*& next();
        Node* const& prev() const;
        Node*& prev();
    };

    struct Iterator {
       private:
        Node* _ptr;
        Node *_head, *_foot;

       public:
        // 构造函数
        Iterator(Node* ptr, Node* head, Node* foot) : _ptr(ptr), _head(head), _foot(foot) {}

        // 重载函数
        T const& operator*() const;
        T& operator*();
        Iterator& operator++();
        Iterator operator++(int);
        Iterator& operator--();
        Iterator operator--(int);
        bool operator==(Iterator const& ptr) const;
        bool operator!=(Iterator const& ptr) const;
    };

    // 内部变量
    Node *_head, *_foot;
    size_t _size;

    // 构造函数与析构函数
    LinkedList() : _head(new Node()), _foot(new Node()), _size(0) {
        _head->next() = _foot, _foot->prev() = _head;
    }
    LinkedList(size_t size);
    LinkedList(size_t size, T const& val);
    ~LinkedList();

    // 迭代器与 C 风格数组操作符
    Iterator begin() { return Iterator(_head->next(), _head, _foot); }
    Iterator end() { return Iterator(_foot, _head, _foot); }
    T& operator[](size_t index);

    // 关于 size_t 的元数据监测
    size_t size() { return _size; }
    bool is_empty() { return _size == 0; }

    // 获取首项与末项元素检测
    T& front() { if (_size == 0) { throw std::out_of_range("LinkedList: out of range"); } return _head->next()->val(); }
    T& back() { if (_size == 0) { throw std::out_of_range("LinkedList: out of range"); } return _foot->prev()->val(); }

    // 首节点(尾节点)下(上)一节点设置
    void push_front(T const& val);
    void push_back(T const& val);

    // 插入函数
    void insert(size_t index, T const& val);

    // 删除函数
    void remove(size_t index);

    // 查找函数
    Iterator find(T const& val);
};


/*
 * 内部 Node 访问器函数
 * 保持访问器函数的简洁, 所有边界检查依靠自行
 */

template<typename T>
T const& LinkedList<T>::Node::val() const { return this->_value; }
template<typename T>
T& LinkedList<T>::Node::val() { return this->_value; }

template<typename T>
LinkedList<T>::Node* const& LinkedList<T>::Node::next() const { return this->_next; }
template<typename T>
LinkedList<T>::Node* &LinkedList<T>::Node::next() { return this->_next; }

template<typename T>
LinkedList<T>::Node* const& LinkedList<T>::Node::prev() const { return this->_prev; }
template<typename T>
LinkedList<T>::Node* &LinkedList<T>::Node::prev() { return this->_prev; }


/* 
 * 内部迭代器 Iterator 重载函数
 */

// 该函数使用前必须确保值有意义
template<typename T>
T const& LinkedList<T>::Iterator::operator*() const { return this->_ptr->val(); }
template<typename T>
T& LinkedList<T>::Iterator::operator*() { return this->_ptr->val(); }

template<typename T>
LinkedList<T>::Iterator& LinkedList<T>::Iterator::operator++() {
    _ptr = (_ptr != _foot ? _ptr->next() : _ptr);
    return *this;
}

template<typename T>
LinkedList<T>::Iterator LinkedList<T>::Iterator::operator++(int) {
    LinkedList<T>::Iterator temp(*this);
    _ptr = (_ptr != _foot ? _ptr->next() : _ptr);
    return temp;
}

template<typename T>
LinkedList<T>::Iterator& LinkedList<T>::Iterator::operator--() {
    _ptr = (_ptr->prev() != _head ? _ptr->prev() : _ptr);
    return *this;
}

template<typename T>
LinkedList<T>::Iterator LinkedList<T>::Iterator::operator--(int) {
    LinkedList<T>::Iterator temp(*this);
    _ptr = (_ptr->prev() != _head ? _ptr->prev() : _ptr);
    return *this;
}

template<typename T>
bool LinkedList<T>::Iterator::operator==(LinkedList<T>::Iterator const& rhs) const {
    return (this->_ptr == rhs._ptr);
}

template<typename T>
bool LinkedList<T>::Iterator::operator!=(LinkedList<T>::Iterator const& rhs) const {
    return (this->_ptr != rhs._ptr);
}


/* 
 * LinkedList 基本构造函数与析构函数
 */

template<typename T>
LinkedList<T>::LinkedList(size_t size) : _head(new Node()), _foot(new Node()), _size(size) {
    Node* prev = _head;
    while (size--) {
        Node* next = new Node();
        prev->next() = next, next->prev() = prev;
        prev = next;
    }
    prev->next() = _foot, _foot->prev() = prev;
}
template<typename T>
LinkedList<T>::LinkedList(size_t size, T const& val) : _head(new Node()), _foot(new Node()), _size(size) {
    Node* prev = _head;
    while (size--) {
        Node* next = new Node(val);
        prev->next() = next, next->prev() = prev;
        prev = next;
    }
    prev->next() = _foot, _foot->prev() = prev;
}

template<typename T>
LinkedList<T>::~LinkedList() {
    for (Node* p = _head; p != nullptr; ) { Node* q = p->next(); delete p; p = q; }
    this->_size = 0, this->_head = nullptr, this->_foot = nullptr;
}


/* 
 * C 风格数组操作(附有边界检查)
 */

template<typename T>
T& LinkedList<T>::operator[](size_t index) {
    if (index >= this->_size) {
        throw std::out_of_range("LinkedList: out of range"); 
    }
    Node* temp = _head->next();
    while(index--) { temp = (temp->next()); }
    return temp->val();
}


/* 
 * 首节点(尾节点)下(上)一节点设置
 */

template<typename T>
void LinkedList<T>::push_front(T const& val) {
    Node* ptr = new Node(val), *temp = _head->next();
    _head->next() = ptr, ptr->next() = temp,
    ptr->prev() = _head, temp->prev() = ptr;
    _size++;
}

template<typename T>
void LinkedList<T>::push_back(T const& val) {
    Node* ptr = new Node(val), *temp = _foot->prev();
    _foot->prev() = ptr, ptr->prev() = temp,
    ptr->next() = _foot, temp->next() = ptr;
    _size++;
}


/*
 * 节点插入设置
 */

template<typename T>
void LinkedList<T>::insert(size_t index, T const& val) {
    if (index > this->_size) {
        throw std::out_of_range("LinkedList: out of range"); 
    }
    Node* temp = _head->next(), *new_node = new Node(val);
    while (index--) { temp = (temp->next()); }
    Node* ltemp = temp->prev();
    ltemp->next() = new_node, new_node->prev() = ltemp,
    new_node->next() = temp, temp->prev() = new_node;
    _size++;
}


/*
 * 节点删除设置
 */

template<typename T>
void LinkedList<T>::remove(size_t index) {
    if (index >= this->_size) {
        throw std::out_of_range("LinkedList: out of range"); 
    }
    Node* temp = _head->next();
    while (index--) { temp = (temp->next()); }
    temp->prev()->next() = temp->next(), 
    temp->next()->prev() = temp->prev();
    delete temp;
    _size--;
}


/*
 * 节点查找设置
 */

template<typename T>
LinkedList<T>::Iterator LinkedList<T>::find(T const& val) {
    for (auto it = this->begin(); it != this->end(); ++it) {
        if (*it == val) {
            return it;
        }
    }
    return this->end();
}