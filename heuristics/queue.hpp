using namespace std;

// Linked list queue

#ifndef QUEUE_HPP
#define QUEUE_HPP

template <typename T>
struct Node {
    T data;
    Node<T>* next;
    Node<T>* prev;
};

template <typename T>
class LinkedList {

protected:

    Node<T>* first;
    Node<T>* last;
    unsigned int N; // number of elements in the linked list

public:
    
    // constructor
    LinkedList() : first(nullptr), last(nullptr), N(0) {}

    bool isEmpty(){

        return N == 0;
    }

    unsigned int getN(){
        return N;
    }

    void addElement(Node<T>* node){

        node->next = nullptr;
        node->prev = last;

        // check if there is a last
        if(last) {

            last->next = node;
        }
        // list is empty
        else{

            first = node;
        }

        last = node;
        N ++;
    }

    Node<T>* getElementByIndex(unsigned int index){

        unsigned int i = 0;

        if(index >= N){

            return nullptr;
        }

        Node<T>* node = first;
        while(node){

            if(i==index){
            
                return node;
            }

            node = node->next;
            i++;
        }

        return nullptr;
    }

    void removeElement(Node<T> *node){

        if(N==0)
            return;

        if(node->prev){

            node->prev->next = node->next;
        }
        else{

            first = node->next;
        }

        if(node->next){

            node->next->prev = node->prev;
        }
        else{

            last = node->prev;
        }

        node->next = nullptr;
        node->prev = nullptr;
        N --;
    }

    Node<T>* removeElementByIndex(unsigned int index){

        Node<T>* node = getElementByIndex(index);
        if(node)
            removeElement(node);

        return node;
    }
};

#endif