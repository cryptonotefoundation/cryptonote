// Copyright (c) 2017-2018, Karbo developers
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <functional>
#include <stdexcept>

namespace {

// Exception thrown on attempt to access an uninitialized Lazy
struct uninitialized_lazy_exception : public std::runtime_error
{
    uninitialized_lazy_exception()
        : std::runtime_error("Uninitialized lazy value.")
    {}
};

template<typename T>
struct Lazy
{
    // Default constructor
    Lazy()
        : m_bInitialized(false)
        , m_initializer(DefaultInitializer)
        , m_deinitializer(DefaultDeinitializer)
    {}

    // Construct with initializer and optional deinitializer functor
    Lazy(std::function<T(void)> initializer, std::function<void(T&)> deinitializer = DefaultDeinitializer)
        : m_bInitialized(false)
        , m_initializer(initializer)
        , m_deinitializer(deinitializer)
    {}

    // Copy constructor
    Lazy(const Lazy& o)
        : m_bInitialized(false)
        , m_initializer(o.m_initializer)
        , m_deinitializer(o.m_deinitializer)
    {
        if (o.m_bInitialized)
            construct(*o.valuePtr());
    }

    // Assign from Lazy<T>
    Lazy& operator=(const Lazy<T>& o)
    {
        destroy();
        m_initializer   = o.m_initializer;
        m_deinitializer = o.m_deinitializer;
        if (o.m_bInitialized)
            construct(*o.valuePtr());
        return *this;
    }

    // Construct from T
    Lazy(const T& v)
        : m_bInitialized(false)
        , m_initializer(DefaultInitializer)
        , m_deinitializer(DefaultDeinitializer)
    {
        construct(v);
    }

    // Assign from T
    T& operator=(const T& value)
    {
        construct(value);
        return *valuePtr();
    }

    // Destruct and deinitialize
    ~Lazy()
    {
        destroy();
    }

    // Answer true if initialized, either implicitly via function or explicitly via assignment
    bool isInitialized() const
    {
        return m_bInitialized;
    }

    // Force initialization, if not already done, and answer with the value
    // Throws exception if not implicitly or explicitly initialized
    T& force() const
    {
        if (!m_bInitialized)
            construct(m_initializer());
        return *valuePtr();
    }

    // Implicitly force initialization and answer with value
    operator T&() const
    {
        return force();
    }

    // Get pointer to storage of T, regardless of initialized state
    T* operator &() const
    {
        return valuePtr();
    }

    // Force initialization state to true, e.g. if value initialized directly via pointer
    void forceInitialized()
    {
        m_bInitialized = true;
    }

private:
    mutable char            m_value[sizeof(T)];
    mutable bool            m_bInitialized;
    std::function<T(void)>  m_initializer;
    std::function<void(T&)> m_deinitializer;

    // Get pointer to storage of T
    T* valuePtr() const
    {
        return static_cast<T*>(static_cast<void*>(&m_value));
    }

    // Call copy constructor for T.  Deinitialize self first, if necessary.
    void construct(const T& value) const
    {
        destroy();
        new (valuePtr()) T(value);
        m_bInitialized = true;
    }
    void construct(T&& value) const
    {
        destroy();
        new (valuePtr()) T(std::move(value));
        m_bInitialized = true;
    }

    // If initialized, call deinitializer and then destructor for T
    void destroy() const
    {
        if (m_bInitialized)
        {
            m_deinitializer(*valuePtr());
            valuePtr()->~T();
            m_bInitialized = false;
        }
    }

    // Inititializer if none specified; throw exception on attempt to access uninitialized lazy
    static T DefaultInitializer()
    {
        throw uninitialized_lazy_exception();
    }

    // Deinitialize if none specified; does nothing
    static void DefaultDeinitializer(T&)
    {
    }
};

}