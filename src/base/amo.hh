/*
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 */

#ifndef __BASE_AMO_HH__
#define __BASE_AMO_HH__

struct AtomicOpFunctor
{
    virtual void operator()(uint8_t *p) = 0;
    virtual AtomicOpFunctor* clone() = 0;
    virtual ~AtomicOpFunctor() {}
};

template <class T>
struct TypedAtomicOpFunctor : public AtomicOpFunctor
{
    void operator()(uint8_t *p) { execute((T *)p); }
    virtual AtomicOpFunctor* clone() = 0;
    virtual void execute(T * p) = 0;
};

template<typename T>
class AtomicGeneric2Op : public TypedAtomicOpFunctor<T>
{
  public:
    AtomicGeneric2Op(T _a, std::function<void(T*,T)> _op)
        : a(_a), op(_op)
    {}
    AtomicOpFunctor* clone() override
    {
        return new AtomicGeneric2Op<T>(*this);
    }
    void execute(T *b) override
    {
        op(b, a);
    }
  private:
    T a;
    std::function<void(T*,T)> op;
 };

template<typename T>
class AtomicGeneric3Op : public TypedAtomicOpFunctor<T>
{
  public:
    AtomicGeneric3Op(T _a, T _c, std::function<void(T*, T, T)> _op)
        : a(_a), c(_c), op(_op)
    {}
    AtomicOpFunctor* clone() override
    {
        return new AtomicGeneric3Op<T>(*this);
    }
    void execute(T *b) override
    {
        op(b, a, c);
    }
  private:
    T a;
    T c;
    std::function<void(T*, T, T)> op;
};

template<typename T>
class AtomicGenericPair3Op : public TypedAtomicOpFunctor<T>
{
  public:
    AtomicGenericPair3Op(std::array<T, 2>& _a, std::array<T, 2> _c,
           std::function<void(T*, std::array<T, 2>&, std::array<T, 2>)> _op)
        : a(_a), c(_c), op(_op)
    {}
    AtomicOpFunctor* clone() override
    {
        return new AtomicGenericPair3Op<T>(*this);
    }
    void execute(T* b) override
    {
        op(b, a, c);
    }
  private:
    std::array<T, 2> a;
    std::array<T, 2> c;
    std::function<void(T*, std::array<T, 2>&, std::array<T, 2>)> op;
};

typedef std::unique_ptr<AtomicOpFunctor> AtomicOpFunctorPtr;

#endif // __BASE_AMO_HH__
