

#ifndef NTL_SmartPtr__H
#define NTL_SmartPtr__H

#include <NTL/tools.h>
#include <NTL/new.h>
#include <NTL/thread.h>



NTL_OPEN_NNS



/****************************************************************************

SmartPtr: a smart pointer class.

Synopsis: provides a reference counted smart pointer, similar to shared_ptr
in the standard library.  It is provided here to minimize reliance
on the standard library, especially for older C++ compilers, which may
not provide shared_ptr, or it may be in TR1, which gets messy.


Examples:


  SmartPtr<T> p1;         // initialize to null
  SmartPtr<T> p2 = 0;

  SmartPtr<T> p3(p1);     // copy constructor

  T *rp;
  SmartPtr<T> p4(rp);     // construct using raw pointer (explicit): better 
                          // to use MakeSmart below

  p1 = MakeSmart<T>(...); // build new T object by invoking constructor
                          // T(...) with pseudo-variadic templates.
                          // This is safer and more efficient that
                          // using the raw-pointer constructor
                        
  p1 = p2;                // assignment
  p1 = 0;                 // assign null


  if (!p1) ...            //  test for null
  if (p1 == 0) ... 

  if (p1) ...             // test for not null ... 
  if (p1 != 0) ... 

  if (p1 == p2) ...       // test for equality 
  if (p1 != p2) 

  *p1                     // dereferencing
  p1->...

  p1.get();               // return the underlying raw pointer...dangerous!

  p1.swap(p2);            // fast swap
  swap(p1, p2);


Automatic Conversions:

If S is another class, SmartPtr<S> converts to SmartPtr<T> if S* converts to T*
(for example, if S is a subclass of T).  Similarly, SmartPtr<S> and SmartPtr<T>
may be compared if S* and T* may be compared.

MakeSmart:

One can write SmartPtr<T> p = MakeSmart<T>(x1, ..., xn), and this will create a
smart pointer to an object constructed as T(x1, ..., xn).  Besides notational
convenience, it also reduces the number of memory allocations from 2 to 1, as
the data and control block can be allocated in one chunck of memory.

This is implemented without reliance on C++11 features, which means that there
are limitations.  First, the number n of arguments is limited to 9.  And
second, all arguments are pass by const reference. However, you can work around
this by using the helper function Fwd.  For example, if T has a 2-argument
constructor where the second must be a non-const reference of some type, and x2
is a variable of that type, you can write MakeSmart<T>(x1, Fwd(x2)), to forward
that reference through all the template nonsense in a typesafe manner.

MakeRaw:

One can also write T *p = MakeRaw<T>(x1, ..., xn) to create a 
raw pointer.  This is the same as writing T *p = new T(x1, ..., xn),
except that if the construction fails, NTL's Error routine will be called
(as opposed to an exception being thrown).  The same restrictions and
limitations that apply to MakeSmart appy to MakeRaw.

Dynamic casting:

I've also supplied a dynamic cast operation for smart pointers.

   SmartPtr<Derived> d = MakeSmart<Derived>(); // d points to Derived
   SmartPtr<Base> b = d; // implicit upcast: OK

   SmartPtr<Derived> d1 = DynamicCast<Derived>(b);
      // downcast to a Derived object -- returns null for a bad cast
   



Implementation notes:

If NTL is compiled with the NTL_THREADS option, then the reference counting
should be thread safe.

The SmartPtrControl class heirarchy is used to make sure the right destructor
is called when the ref count goes to zero.  This can be an issue for forward
declared classes and for subclasses.  For example, if T is forward declared in
a context where the ref count goes to zero, or if the object's actual type is a
subclass of T and T's destructor was not declared virtual.

The null tests p, !p, p == 0, are all affected via an implicit conversion from
SmartPtr<T> to a funny pointer type (a pointer to a member function, which
avoids other, unwanted implicit conversions: this is the so-called "safe bool
idiom");

Also, there is an implicit conversion from the same, funny pointer type to
SmartPtr<T>, which is how one can use 0 to initialize and assign to a
SmartPtr<T>.

In C++11 both of the above effects could perhaps be achieved more directly.
The new "explict bool" operator can replace the "safe bool idiom", and I would
think that the new null pointer could be used to get the conversion from "0" to
work.

NOTES: See http://www.artima.com/cppsource/safebool.html for more
on the "safe bool idiom".  

 


*****************************************************************************/


class SmartPtrControl {
public:
   AtomicRefCount cnt;
   SmartPtrControl() { }
   virtual ~SmartPtrControl() { }


private:
   void operator=(const SmartPtrControl&); // =delete
   SmartPtrControl(const SmartPtrControl&); // =delete
};



template<class T>
class SmartPtrControlDerived : public SmartPtrControl {
public:
   T* p;

   SmartPtrControlDerived(T* _p) : p(_p)  { }

   ~SmartPtrControlDerived() 
   { 
      delete p; 
   }

};

 
struct SmartPtrLoopHole { };


template<class T>
class SmartPtr {
private:
   T *dp;
   SmartPtrControl *cp;

   void AddRef() const
   {
      if (cp) cp->cnt.inc();
   }

   void RemoveRef() const
   {
      if (cp && cp->cnt.dec()) { delete cp; }
   }

   class Dummy { };

   typedef void (SmartPtr::*fake_null_type)(Dummy) const;
   void fake_null_function(Dummy) const {}


public:
   explicit SmartPtr(T* p) : dp(p), cp(0) 
   {
      if (dp) {
         cp = NTL_NEW_OP SmartPtrControlDerived<T>(dp);
         if (!cp) NTL_NNS Error("out of memory in SmartPtr");
         AddRef();
      }
   }

   SmartPtr() : dp(0), cp(0)  { }

   SmartPtr(SmartPtrLoopHole, T* _dp, SmartPtrControl *_cp) : dp(_dp), cp(_cp)
   { 
      AddRef();
   } 

   ~SmartPtr() {
      RemoveRef();
   }

   SmartPtr(const SmartPtr& other) : dp(other.dp), cp(other.cp) 
   {
      AddRef();
   }


   SmartPtr& operator=(const SmartPtr& other)
   {
      other.AddRef();
      RemoveRef();
      dp = other.dp;
      cp = other.cp;
      return *this;
   }

   template<class Y> friend class SmartPtr;

   template<class Y>
   SmartPtr(const SmartPtr<Y>& other) : dp(other.dp), cp(other.cp) 
   {
      AddRef();
   }


   template<class Y>
   SmartPtr& operator=(const SmartPtr<Y>& other)
   {
      other.AddRef();
      RemoveRef();
      dp = other.dp;
      cp = other.cp;
      return *this;
   }


   T& operator*()  const { return *dp; }
   T* operator->() const { return dp; }

   T* get() const { return dp; }

   void swap(SmartPtr& other)
   {
      T *t_dp = dp; dp = other.dp; other.dp = t_dp; 
      SmartPtrControl *t_cp = cp; cp = other.cp; other.cp = t_cp; 
   }

   SmartPtr(fake_null_type) : dp(0), cp(0) { }

   operator fake_null_type() const 
   {
      return dp ?  &SmartPtr::fake_null_function : 0;
   }


   template<class Y> 
   SmartPtr<Y> DynamicCast() const 
   {
      if (!dp) 
         return SmartPtr<Y>();
      else {
         Y* dp1 = dynamic_cast<Y *>(dp);
         if (!dp1) return SmartPtr<Y>();
         return SmartPtr<Y>(SmartPtrLoopHole(), dp1, cp);
      }
   }


};


// free swap function
template<class T>
void swap(SmartPtr<T>& p, SmartPtr<T>& q) { p.swap(q); }

// free dynamic cast function
template<class X, class Y>
SmartPtr<X> DynamicCast(const SmartPtr<Y>& p) { return p.DynamicCast<X>(); }



// Equality testing

template<class X, class Y>
bool operator==(const SmartPtr<X>& a, const SmartPtr<Y>& b)
{
   return a.get() == b.get();
}

template<class X, class Y>
bool operator!=(const SmartPtr<X>& a, const SmartPtr<Y>& b)
{
   return a.get() != b.get();
}


/*********************************************************************************

Experimantal: CloneablePtr<T> ...essentially same interface as SmartPtr, but 
allows cloning of complete objects.  The differences:
*  must construct using MakeCloneable
*  a clone method is provided
*  implicit conversion from CloneablePtr to SmartPtr is allowed

Example:

   CloneablePtr<Derived> d = MakeCloneable<Derived>(); // d points to Derived
   CloneablePtr<Base> b = d; // implicit upcast: OK

   CloneablePtr<Base> b1 = b.clone(); // clone of b, which is really a Derived object
   CloneablePtr<Derived> d1 = DynamicCast<Derived>(b1);
      // downcast to a Derived object -- returns null for a bad cast
   SmartPtr<Base> b2 = d1;
   


Implementation:

In the clone method, the object is constructed using the copy constructor for the 
type T, where T is the compile-time type with which the first smart pointer to this
object was was created, even if the pointer has been subsequently upcasted to a
base type S.  Such objects must have been initially created using the
MakeCloneable function.  It turns out, this is hard to do in a completely
standards-compliant way, because of the type erasure going on.  The only way I
could figure out how to do it in a standards-compliant way was by using
exceptions: the control block throws a T* and the smart pointer doing the clone
catches an S*.  However, this turned out to be dreadfully slow, and even this
does not completely solve the problem, because there could be ambiguities
in this type of upcasting that miay arise with multiple inheritance.  So I settled
on the current method, which does some low-level pointer arithmetic.  Even with
fancy things like multiple and virtual inheritance, it should work, under the
assumption that if two objects have the same (runtime) type, then their memory
layout is the same.  I don't think anything like that is guaranteed by the
standard, but this seems reasonable, and it seems to work.
Like I said, it is experimental, and I would appreciate feedback
from C++ gurus.

Note that NTL does not use this feature, but I do have applications where this
is convenient.


**********************************************************************************/

class CloneablePtrControl : public SmartPtrControl {
public:
   virtual CloneablePtrControl *clone() const = 0;
   virtual void *get() = 0;

};


template<class T>
class CloneablePtrControlDerived : public CloneablePtrControl {
public:
   T d;

   CloneablePtrControlDerived(const T& x) : d(x) { }
   CloneablePtrControl *clone() const 
   {
      CloneablePtrControl *q = NTL_NEW_OP CloneablePtrControlDerived<T>(d);
      if (!q) NTL_NNS Error("out of mem in clone");
      return q;
   }
   void *get() { return &d; }
};



 
struct CloneablePtrLoopHole { };


template<class T>
class CloneablePtr {
private:
   T *dp;
   CloneablePtrControl *cp;

   void AddRef() const
   {
      if (cp) cp->cnt.inc();
   }

   void RemoveRef() const
   {
      if (cp && cp->cnt.dec()) { delete cp; }
   }

   class Dummy { };

   typedef void (CloneablePtr::*fake_null_type)(Dummy) const;
   void fake_null_function(Dummy) const {}


public:
   CloneablePtr() : dp(0), cp(0)  { }

   CloneablePtr(CloneablePtrLoopHole, T* _dp, CloneablePtrControl *_cp) : dp(_dp), cp(_cp)
   { 
      AddRef();
   } 

   ~CloneablePtr() {
      RemoveRef();
   }

   CloneablePtr(const CloneablePtr& other) : dp(other.dp), cp(other.cp) 
   {
      AddRef();
   }


   CloneablePtr& operator=(const CloneablePtr& other)
   {
      other.AddRef();
      RemoveRef();
      dp = other.dp;
      cp = other.cp;
      return *this;
   }

   template<class Y> friend class CloneablePtr;

   template<class Y>
   CloneablePtr(const CloneablePtr<Y>& other) : dp(other.dp), cp(other.cp) 
   {
      AddRef();
   }


   template<class Y>
   CloneablePtr& operator=(const CloneablePtr<Y>& other)
   {
      other.AddRef();
      RemoveRef();
      dp = other.dp;
      cp = other.cp;
      return *this;
   }


   T& operator*()  const { return *dp; }
   T* operator->() const { return dp; }

   T* get() const { return dp; }

   void swap(CloneablePtr& other)
   {
      T *t_dp = dp; dp = other.dp; other.dp = t_dp; 
      CloneablePtrControl *t_cp = cp; cp = other.cp; other.cp = t_cp; 
   }

   CloneablePtr(fake_null_type) : dp(0), cp(0) { }

   operator fake_null_type() const 
   {
      return dp ?  &CloneablePtr::fake_null_function : 0;
   }

   template<class Y> 
   CloneablePtr<Y> DynamicCast() const 
   {
      if (!dp) 
         return CloneablePtr<Y>();
      else {
         Y* dp1 = dynamic_cast<Y *>(dp);
         if (!dp1) return CloneablePtr<Y>();
         return CloneablePtr<Y>(CloneablePtrLoopHole(), dp1, cp);
      }
   }

   CloneablePtr clone() const 
   {
      if (!dp) 
         return CloneablePtr();
      else {
         CloneablePtrControl *cp1 = cp->clone();
         char *complete = (char *) cp->get();
         char *complete1 = (char *) cp1->get();
         T *dp1 = (T *) (complete1 + (((char *)dp) - complete));
         return CloneablePtr(CloneablePtrLoopHole(), dp1, cp1);
      }
   }

   template<class Y>
   operator SmartPtr<Y>() { return SmartPtr<Y>(SmartPtrLoopHole(), dp, cp); }

};


// free swap function
template<class T>
void swap(CloneablePtr<T>& p, CloneablePtr<T>& q) { p.swap(q); }

// free dynamic cast function
template<class X, class Y>
CloneablePtr<X> DynamicCast(const CloneablePtr<Y>& p) { return p.DynamicCast<X>(); }



// Equality testing

template<class X, class Y>
bool operator==(const CloneablePtr<X>& a, const CloneablePtr<Y>& b)
{
   return a.get() == b.get();
}


template<class X, class Y>
bool operator!=(const CloneablePtr<X>& a, const CloneablePtr<Y>& b)
{
   return a.get() != b.get();
}




// ******************************************************

// Implementation of MakeSmart and friends


// Reference forwarding

template<class T>
class ReferenceWrapper
{
private:
   T& x;
public:
   ReferenceWrapper(T& _x) : x(_x) { }
   operator T& () const { return x; }
};

template<class T> 
ReferenceWrapper<T> Fwd(T& x) { return ReferenceWrapper<T>(x); }

template<class T>
class ConstReferenceWrapper
{
private:
   T& x;
public:
   ConstReferenceWrapper(const T& _x) : x(_x) { }
   operator const T& () const { return x; }
};

template<class T> 
ConstReferenceWrapper<T> Fwd(const T& x) { return ConstReferenceWrapper<T>(x); }

template<class T>
T& UnwrapReference(const ReferenceWrapper<T>& x) { return x; } 

template<class T>
const T& UnwrapReference(const ConstReferenceWrapper<T>& x) { return x; } 

template<class T>
const T& UnwrapReference(const T& x) { return x; } 




// Some useful macros for simulating variadic templates

#define NTL_REPEATER_0(m) 
#define NTL_REPEATER_1(m)  m(1)
#define NTL_REPEATER_2(m)  m(1),m(2)
#define NTL_REPEATER_3(m)  m(1),m(2),m(3)
#define NTL_REPEATER_4(m)  m(1),m(2),m(3),m(4)
#define NTL_REPEATER_5(m)  m(1),m(2),m(3),m(4),m(5)
#define NTL_REPEATER_6(m)  m(1),m(2),m(3),m(4),m(5),m(6)
#define NTL_REPEATER_7(m)  m(1),m(2),m(3),m(4),m(5),m(6),m(7)
#define NTL_REPEATER_8(m)  m(1),m(2),m(3),m(4),m(5),m(6),m(7),m(8)
#define NTL_REPEATER_9(m)  m(1),m(2),m(3),m(4),m(5),m(6),m(7),m(8),m(9)

#define NTL_SEPARATOR_0 
#define NTL_SEPARATOR_1  ,
#define NTL_SEPARATOR_2  ,
#define NTL_SEPARATOR_3  ,
#define NTL_SEPARATOR_4  ,
#define NTL_SEPARATOR_5  ,
#define NTL_SEPARATOR_6  ,
#define NTL_SEPARATOR_7  ,
#define NTL_SEPARATOR_8  ,
#define NTL_SEPARATOR_9  ,


#define NTL_FOREACH_ARG(m) \
   m(0) m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9)

#define NTL_FOREACH_ARG1(m) \
   m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9)

// ********************************

#define NTL_ARGTYPE(n) class X##n
#define NTL_ARGTYPES(n) NTL_REPEATER_##n(NTL_ARGTYPE)
#define NTL_MORE_ARGTYPES(n) NTL_SEPARATOR_##n NTL_REPEATER_##n(NTL_ARGTYPE)

#define NTL_VARARG(n) const X##n & x##n
#define NTL_VARARGS(n) NTL_REPEATER_##n(NTL_VARARG)

#define NTL_PASSTYPE(n) X ## n
#define NTL_PASSTYPES(n) NTL_REPEATER_##n(NTL_PASSTYPE)
#define NTL_MORE_PASSTYPES(n) NTL_SEPARATOR_##n NTL_REPEATER_##n(NTL_PASSTYPE)

#define NTL_PASSARG(n) x ## n
#define NTL_PASSARGS(n) NTL_REPEATER_##n(NTL_PASSARG)

#define NTL_UNWRAPARG(n) UnwrapReference(x ## n)
#define NTL_UNWRAPARGS(n) NTL_REPEATER_##n(NTL_UNWRAPARG)



// ********************************



#define NTL_DEFINE_MAKESMART(n) \
template<class T  NTL_MORE_ARGTYPES(n)> \
class MakeSmartAux##n : public SmartPtrControl {\
public: T d; \
MakeSmartAux##n( NTL_VARARGS(n) ) : \
d( NTL_UNWRAPARGS(n) ) { }\
};\
\
template<class T NTL_MORE_ARGTYPES(n)>\
SmartPtr<T> MakeSmart( NTL_VARARGS(n) ) { \
   MakeSmartAux##n<T NTL_MORE_PASSTYPES(n) > *cp = \
   NTL_NEW_OP MakeSmartAux##n<T NTL_MORE_PASSTYPES(n)>( NTL_PASSARGS(n) ); \
   if (!cp) NTL_NNS Error("out of memory in MakeSmart");\
   return SmartPtr<T>(SmartPtrLoopHole(), &cp->d, cp);\
};\

NTL_FOREACH_ARG(NTL_DEFINE_MAKESMART)




// ********************************


#define NTL_DEFINE_MAKECLONEABLE(n) \
template<class T  NTL_MORE_ARGTYPES(n)> \
class MakeCloneableAux##n : public CloneablePtrControl {\
public: T d; \
MakeCloneableAux##n( NTL_VARARGS(n) ) : \
d( NTL_UNWRAPARGS(n) ) { }\
CloneablePtrControl *clone() const \
{\
   CloneablePtrControl *q = NTL_NEW_OP CloneablePtrControlDerived<T>(d);\
   if (!q) NTL_NNS Error("out of mem in clone");\
   return q;\
}\
void *get() { return &d; }\
};\
\
template<class T NTL_MORE_ARGTYPES(n)>\
CloneablePtr<T> MakeCloneable( NTL_VARARGS(n) ) { \
   MakeCloneableAux##n<T NTL_MORE_PASSTYPES(n) > *cp = \
   NTL_NEW_OP MakeCloneableAux##n<T NTL_MORE_PASSTYPES(n)>( NTL_PASSARGS(n) ); \
   if (!cp) NTL_NNS Error("out of memory in MakeCloneable");\
   return CloneablePtr<T>(CloneablePtrLoopHole(), &cp->d, cp);\
};\

NTL_FOREACH_ARG(NTL_DEFINE_MAKECLONEABLE)



// ********************************



#define NTL_DEFINE_MAKERAW(n)\
template<class T  NTL_MORE_ARGTYPES(n)>\
T* MakeRaw(NTL_VARARGS(n)) { \
   T *p = NTL_NEW_OP T(NTL_UNWRAPARGS(n)); \
   if (!p) NTL_NNS Error("out of memory in MakeRaw");\
   return p;\
};\


NTL_FOREACH_ARG(NTL_DEFINE_MAKERAW)


// ********************************





/**********************************************************************

UniquePtr<T> -- unique pointer to object with copying disabled.
Useful for pointers inside classes so that we can
automatically destruct them.  

Constructors:
   UniquePtr<T> p1;     // initialize with null

   T* rp;
   UniquePtr<T> p4(rp); // construct using raw pointer (explicit)

   p1 = 0;              // destroy's p1's referent and assigns null

   p1.make(...);        // destroy's p1's referent and assigns
                        // a fresh objected constructed via T(...),
                        // using psuedo variadic templates
                
   p1.reset(rp);        // destroy's p1's referent and assign rp

   if (!p1) ...         // test for null
   if (p1 == 0) ...

   if (p1) ...          // test for nonnull
   if (p1 != 0) ...

   if (p1 == p2) ...    // test for equality
   if (p1 != p2) ...   

   *p1                  // dereferencing
   p1->...


   rp = p1.get();       // fetch raw pointer
   rp = p1.release();   // fetch raw pointer, and set to NULL
   p1.move(p2);         // equivalent to p1.reset(p2.release()) --
                        // if p1 != p2 then:
                        //    makes p1 point to p2's referent,
                        //    setting p2 to NULL and destroying
                        //    p1's referent

   p1.swap(p2);         // fast swap
   swap(p1, p2);

   
**********************************************************************/


template<class T>
class UniquePtr {
private:
   T *dp;

   class Dummy { };

   typedef void (UniquePtr::*fake_null_type)(Dummy) const;
   void fake_null_function(Dummy) const {}

   bool cannot_compare_these_types() const { return false; }

   UniquePtr(const UniquePtr&); // disabled
   void operator=(const UniquePtr&); // disabled

public:   
   explicit UniquePtr(T *p) : dp(p) { }

   UniquePtr() : dp(0) { }

   ~UniquePtr() { delete dp; }

   void reset(T* p = 0)
   {
      if (dp == p) return;
      delete dp;
      dp = p;
   }

   UniquePtr& operator=(fake_null_type) { reset(); }

   void make()
   {
      reset(MakeRaw<T>());
   }

#define NTL_DEFINE_UNIQUE_MAKE(n) \
   template< NTL_ARGTYPES(n) >\
   void make( NTL_VARARGS(n) )\
   {\
      reset(MakeRaw<T>( NTL_PASSARGS(n) ));\
   }\

   NTL_FOREACH_ARG1(NTL_DEFINE_UNIQUE_MAKE)

   T& operator*()  const { return *dp; }
   T* operator->() const { return dp; }

   T* get() const { return dp; }

   T* release() { T *p = dp; dp = 0; return p; }
   void move(UniquePtr& other) { reset(other.release()); }

   void swap(UniquePtr& other)
   {
      T *t_dp = dp; dp = other.dp; other.dp = t_dp; 
   }

   UniquePtr(fake_null_type) : dp(0) { }

   operator fake_null_type() const 
   {
      return dp ?  &UniquePtr::fake_null_function : 0;
   }

};


// free swap function
template<class T>
void swap(UniquePtr<T>& p, UniquePtr<T>& q) { p.swap(q); }



// Equality testing

template<class X>
bool operator==(const UniquePtr<X>& a, const UniquePtr<X>& b)
{
   return a.get() == b.get();
}

template<class X>
bool operator!=(const UniquePtr<X>& a, const UniquePtr<X>& b)
{
   return a.get() != b.get();
}


// the following definitions of == and != prevent comparisons
// on UniquePtr's to different types...such comparisons
// don't make sense...defining these here ensures the compiler
// emits an error message...and a pretty readable one


template<class X, class Y>
bool operator==(const UniquePtr<X>& a, const UniquePtr<Y>& b)
{
   return a.cannot_compare_these_types();
}

template<class X, class Y>
bool operator!=(const UniquePtr<X>& a, const UniquePtr<Y>& b)
{
   return a.cannot_compare_these_types();
}



/**********************************************************************

UniqueArray<T> -- unique pointer to array of objects with copying disabled.
Useful for pointers inside classes so that we can
automatically destruct them.  

Constructors:
   UniqueArray<T> p1;     // initialize with null

   T* rp;
   UniqueArray<T> p4(rp); // construct using raw pointer (explicit)

   p1 = 0;              // destroy's p1's referent and assigns null

   p1.make(n);          // destroy's p1's referent and assigns
                        // a fresh objected constructed via new T[n]
                
   p1.reset(rp);        // destroy's p1's referent and assign rp

   if (!p1) ...         // test for null
   if (p1 == 0) ...

   if (p1) ...          // test for nonnull
   if (p1 != 0) ...

   if (p1 == p2) ...    // test for equality
   if (p1 != p2) ...   

   p1[i]                // array indexing

   rp = p1.get();       // fetch raw pointer
   rp = p1.release();   // fetch raw pointer, and set to NULL
   p1.move(p2);         // equivalent to p1.reset(p2.release()) --
                        // if p1 != p2 then:
                        //    makes p1 point to p2's referent,
                        //    setting p2 to NULL and destroying
                        //    p1's referent

   p1.swap(p2);         // fast swap
   swap(p1, p2);

   
**********************************************************************/


template<class T>
class UniqueArray {
private:
   T *dp;

   class Dummy { };

   typedef void (UniqueArray::*fake_null_type)(Dummy) const;
   void fake_null_function(Dummy) const {}

   bool cannot_compare_these_types() const { return false; }

   UniqueArray(const UniqueArray&); // disabled
   void operator=(const UniqueArray&); // disabled

public:   
   explicit UniqueArray(T *p) : dp(p) { }

   UniqueArray() : dp(0) { }

   ~UniqueArray() { delete[] dp; }

   void reset(T* p = 0)
   {
      if (dp == p) return;
      delete[] dp;
      dp = p;
   }

   UniqueArray& operator=(fake_null_type) { reset(); }

   void make(long n)
   {
      if (n <= 0) NTL_NNS Error("argument to UniqueArray::make not positive");
      T *p = NTL_NEW_OP T[n];
      if (!p) NTL_NNS Error("out of mem in UniqueArray::make");
      reset(p);
   }

   T& operator[](long i) const { return dp[i]; }

   T* get() const { return dp; }

   T* release() { T *p = dp; dp = 0; return p; }
   void move(UniqueArray& other) { reset(other.release()); }

   void swap(UniqueArray& other)
   {
      T *t_dp = dp; dp = other.dp; other.dp = t_dp; 
   }

   UniqueArray(fake_null_type) : dp(0) { }

   operator fake_null_type() const 
   {
      return dp ?  &UniqueArray::fake_null_function : 0;
   }

};


// free swap function
template<class T>
void swap(UniqueArray<T>& p, UniqueArray<T>& q) { p.swap(q); }



// Equality testing

template<class X>
bool operator==(const UniqueArray<X>& a, const UniqueArray<X>& b)
{
   return a.get() == b.get();
}

template<class X>
bool operator!=(const UniqueArray<X>& a, const UniqueArray<X>& b)
{
   return a.get() != b.get();
}


// the following definitions of == and != prevent comparisons
// on UniqueArray's to different types...such comparisons
// don't make sense...defining these here ensures the compiler
// emits an error message...and a pretty readable one


template<class X, class Y>
bool operator==(const UniqueArray<X>& a, const UniqueArray<Y>& b)
{
   return a.cannot_compare_these_types();
}

template<class X, class Y>
bool operator!=(const UniqueArray<X>& a, const UniqueArray<Y>& b)
{
   return a.cannot_compare_these_types();
}







NTL_CLOSE_NNS


#endif