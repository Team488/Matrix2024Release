// A base class for cameras or camera-like objects with basic lifecycle methods.

// TODO: DELETE THIS FILE

#ifndef RUNNABLE_H
#define RUNNABLE_H 1

class Runnable {
    public:
        virtual ~Runnable() = default;
        virtual bool start() {return true;}
        virtual bool update() = 0;
};

#endif //ifndef RUNNABLE_H