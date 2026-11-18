/*
Copyright (C) 2026 ZFSM Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/* 
The ZFSM homepage provides the library source code and has all the documentation. It is located at 

    https://mbaldamus.com/zfsm.
*/

#if defined(ZFSM_ERRORS_ASSUME_NONE) && defined(ZFSM_ERRORS_ASSERT)
static_assert(false, "zfsm: ZFSM_ERRORS_ASSUME_NONE and ZFSM_ERRORS_ASSERT defined at the same time");
#endif

#ifndef INCLUDED_ZFSM_HPP
#define INCLUDED_ZFSM_HPP

#if defined(ZFSM_ERRORS_ASSERT)

#include <cassert>

#elif !defined(ZFSM_ERRORS_ASSUME_NONE)

#include <cstdio>
#include <exception>

#endif

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#if defined(ZFSM_ERRORS_ASSUME_NONE)
#define ZFSM_ERROR(_ignore1, _ignore2) ((void) 0)
#elif defined(ZFSM_ERRORS_ASSERT)
#define ZFSM_ERROR(condition, _ignore) assert (!(condition))
#else
#define ZFSM_ERROR(condition, text) do { if (condition) { throw Exception(text); } } while (false)
#endif

namespace zfsm
{

namespace internal
{ 
    enum class ArrowType { REACT, CONTINUE, PREEMPT, IMMEDIATE_PREEMPT };
    enum class ContextActionType { ON_ENTRY, ON_EXIT, DO_FRONT, DO_BACK };

    class ChainedSignal;
    class InjectOnEntry;
    class InjectOnExit;
    class InjectDoFront;
    class InjectDoBack;
    template <class Ctxt> class SubState;
    template <class Elem> class Context; 
    class RegionMixee;
    template <bool> class SignalMixIn;
    template <ContextActionType, bool> class ContextActionMixIn;
    template <ArrowType, bool> class ArrowMixIn;
    class Edge;
    class TopMixee;

} // namespace internal

#if !defined(ZFSM_ERRORS_ASSUME_NONE) && !defined(ZFSM_ERRORS_ASSERT) 
class Exception : public std::exception 
{
    friend class internal::ChainedSignal;
    template <class Elem> friend class internal::Context; 
    friend class internal::InjectOnEntry;
    friend class internal::InjectOnExit;
    friend class internal::InjectDoFront;
    friend class internal::InjectDoBack;
    template <class Ctxt> friend class internal::SubState;
    friend class internal::RegionMixee;
    friend class internal::TopMixee;
    template <bool> friend class internal::SignalMixIn;
    template <internal::ContextActionType, bool> friend class internal::ContextActionMixIn;
    template <internal::ArrowType, bool> friend class internal::ArrowMixIn;
    friend class internal::Edge;

    static int constexpr BUF_CAPACITY = 80;
    char msg[BUF_CAPACITY];

    Exception(char const *text) noexcept { std::snprintf(msg, BUF_CAPACITY, "zfsm exception: %s", text); }

public:
    char const * what() const noexcept final { return msg; }
};
#endif

namespace internal
{
    template <class CRTP>
    class Chained
    {
        friend class ChainedSignal;
        friend class ChainedEdge;
        friend class ChainedContextAction;
        friend class RegionMixee;
        template <ArrowType, bool> friend class ArrowMixIn;
        template <ContextActionType, bool> friend class ContextActionMixIn;
        template <class Elem> friend class Context;
        template <class Ctxt> friend class SubState;
        template <class Ctxt> friend class HierarchicalState;

        Chained() = default;
        Chained(Chained const &) = delete;
        Chained(Chained &&) = delete;
        Chained& operator=(Chained const &) = delete;
        Chained& operator=(Chained &&) = delete;

        virtual ~Chained() = default;

    private:
        CRTP *reverseElemChain() { 
            CRTP *prev = nullptr; 
            CRTP *cur = static_cast<CRTP *>(this); 
            for (;;) { CRTP *const nxt = cur->m_nxt; cur->m_nxt = prev; prev = cur; if (nxt == nullptr) { break; } cur = nxt; } 
            return prev; 
        }
    };

    class Scope;

} // namespace internal

template <typename T = void, class SFINAE = void> class Signal;

namespace internal
{
    class InitMixee;
    class FinalMixee;
    class BasicMixee;
    class SuperMixee;
    class RegionMixee;
    class CompositeMixee;
    class TopMixee;

    class ChainedSignal : Chained<ChainedSignal>
    {
        template <typename T, class SFINAE> friend class zfsm::Signal;
        template <bool> friend class SignalMixIn;

    public:
        class Raised { friend class ChainedSignal; };

        bool operator()() const { return m_isRaised; }

    private:
        bool m_isRaised = false;

        ChainedSignal *m_nxt = nullptr;
        void chain(ChainedSignal *nxtElem) { m_nxt = nxtElem; }

        virtual void reset() { m_isRaised = false; }

        Raised raise() { m_isRaised = true; return Raised(); }

        ChainedSignal(internal::Scope *scope);
    };

} // namespace internal

template <typename T>
class Signal<T, typename std::enable_if<std::is_void<T>::value>::type> : public internal::ChainedSignal 
{
public:
    Signal(internal::Scope *scope) : internal::ChainedSignal(scope) {} 

    ChainedSignal::Raised raise() { return internal::ChainedSignal::raise(); }
};

namespace internal
{
    template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> class DecoratedEdge;
    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action, class SFINAE> class ContextAction;
}

template <typename T>
class Signal<T, typename std::enable_if<!std::is_void<T>::value>::type> : public internal::ChainedSignal 
{
    std::optional<T> m_payload;

    void reset() noexcept override { 
        m_payload.reset(); 
        ChainedSignal::reset();
    }

public:
    Signal(internal::Scope *scope) : internal::ChainedSignal(scope) {}

    ~Signal() { m_payload.reset(); }

    ChainedSignal::Raised raise(T const &value) {
        m_payload = value;        
        return internal::ChainedSignal::raise(); 
    }

    ChainedSignal::Raised raise(T &&value) { 
        m_payload = std::move(value);          
        return internal::ChainedSignal::raise();
    }

    T const &getValue() const { return *m_payload; }
    T&& takeValue() { return std::move(*m_payload); }
};

namespace internal
{
    class ChainedEdge : public Chained<ChainedEdge> 
    {
        friend class Chained<ChainedEdge>;
        template <ArrowType, bool> friend class ArrowMixIn;
        friend class InjectContinue;

        ChainedEdge *m_nxt = nullptr;
        void chain(ChainedEdge *nxtElem) { m_nxt = nxtElem; }

        virtual bool chainedArrow_evalExec(bool immediatePreempt = false) = 0;
    };

    class ChainedContextAction : public Chained<ChainedContextAction>
    {
        friend class Chained<ChainedContextAction>;
        template <ContextActionType, bool> friend class ContextActionMixIn;

        ChainedContextAction *m_nxt = nullptr;
        void chain(ChainedContextAction *nxtElem) { m_nxt = nxtElem; }

        virtual void chainedAction_evalExec() = 0;
    };

    class ChainedOnEntry : public ChainedContextAction {};
    class ChainedOnExit : public ChainedContextAction {};
    class ChainedDoFront : public ChainedContextAction {};
    class ChainedDoBack : public ChainedContextAction {};

    class Scope 
    {
        friend class Exception;
        template <class Elem> friend class Context;
        friend class ChainedSignal;
        friend class InjectOnEntry;
        friend class InjectOnExit;
        friend class InjectDoFront;
        friend class InjectDoBack;
        friend class Sequential;
        friend class SuperMixee;
        friend class RegionMixee;
        friend class CompositeMixee;
        friend class TopMixee;
        friend class Nextifier;

        Scope() = default;
        Scope(Scope const &) = delete;
        Scope(Scope &&) = delete;
        Scope& operator=(Scope const &) = delete;
        Scope& operator=(Scope &&) = delete;

        virtual ~Scope() = default;

        virtual void injectSignal(ChainedSignal *signal) = 0;
        virtual void asScope_nextify() const = 0;

        virtual void injectEntry(ChainedOnEntry *onEntry) = 0;
        virtual void fuseEntries() = 0;
        virtual void onEntry() const = 0;

        virtual void injectExit(ChainedOnExit *onExit) = 0;
        virtual void fuseExits() = 0;
        virtual void onExit() const = 0;

        virtual void injectDoFront(ChainedDoFront *doFront) = 0;
        virtual void fuseDoFronts() = 0;
        virtual void doFront() const = 0;

        virtual void injectDoBack(ChainedDoBack *doBack) = 0;
        virtual void fuseDoBacks() = 0;
        virtual void doBack() const = 0;

        void fuse() { fuseEntries(); fuseExits(); fuseDoFronts(); fuseDoBacks(); }
    };

    inline ChainedSignal::ChainedSignal(Scope *scope) : m_isRaised(false) { ZFSM_ERROR(scope == nullptr, "scope pointer on signal is null"); scope->injectSignal(this); }

    template <class Ctxt>
    class SubState : Chained<SubState<Ctxt>> 
    {
        template <class CRTP> friend class Chained;
        template <class Elem> friend class Context;
        friend class Sequential;
        template <class _Ctxt> friend class HierarchicalState;
        friend class Ground;
        friend class FinalMixee;
        friend class SuperMixee;
        friend class RegionMixee;
        friend class CompositeMixee;
        friend class Edge;
        friend class OnLeaveByMove;
        friend class OnLeaveByAbort;
        friend class OnLeaveBySuspend;
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        Ctxt *const m_ctxt;

        SubState *m_nxt = nullptr;
        void injectNxtElem(SubState *nxtElem) { m_nxt = nxtElem; }

        virtual void injectReact(ChainedEdge *react) = 0;
        virtual void fuseReacts() = 0;
        virtual bool react() = 0;

        virtual void asContextElem_fuse() { fuseReacts(); }

        virtual bool asSubState_isInit() const { return false; }

        virtual bool asSubState_isFinal() const { return false; }

        virtual bool asSubState_react() { return react(); }

        virtual bool asSubState_continue() { 
            ZFSM_ERROR(true, "internal error in asSubState_continue()");
#ifdef ZFSM_ERRORS_ASSUME_NONE
            return false;
#endif
        }

        virtual void asSubState_nextify() {}

        virtual void asSource_exit() {}

        virtual void asTarget_entry() {}

        virtual void asSubState_abort() { ZFSM_ERROR(true, "internal error asSubState_abort()"); }

        virtual void asSubState_suspend() { ZFSM_ERROR(true, "internal error asSubState_suspend()"); } 

        virtual void asSubState_reset() {}

        virtual void asSubState_deepReset() {}

    protected:
        SubState(Ctxt *ctxt) : m_ctxt(ctxt) { ZFSM_ERROR(ctxt == nullptr, "context pointer in sub-state is null"); ctxt->injectElem(this); }
    };

    template <class Elem>
    class Context : public Scope 
    {
        friend class Sequential;
        template <class Context> friend class SubState;
        friend class CompositeMixee;

        Elem *m_fstElem = nullptr;
        void injectElem(Elem *elem) { elem->injectNxtElem(m_fstElem); m_fstElem = elem; }

        virtual void asContext_fuse() {
            Scope::fuse();
            ZFSM_ERROR(m_fstElem == nullptr, "missing context element(s)");
            m_fstElem = m_fstElem->reverseElemChain();
            { auto elem = m_fstElem; do { elem->asContextElem_fuse(); elem = elem->m_nxt; } while (elem != nullptr); }
        }
    };

    class Sequential : public Context<SubState<Sequential>> 
    {
        friend class SuperMixee;
        friend class RegionMixee;
        friend class CompositeMixee;
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;
        friend class TopMixee;

        InitMixee &m_init;
        SubState<Sequential> *m_current;

        void updateCurrent(SubState<Sequential> *newCurrent) { m_current = newCurrent; }

        void asContext_fuse() final;

        bool asSequential_hasTerminated() const { return m_current->asSubState_isFinal(); }

        void asSequential_exit();

        void asSequential_entry() { if (m_current->asSubState_isInit()) { onEntry(); asSequential_continue(); } }

        bool asSequential_coreReact() {
            bool const hasReacted = m_current->asSubState_react();
            if (hasReacted) { asSequential_continue(); }
            return hasReacted;
        }

        bool asSequential_react() {
            if (asSequential_hasTerminated()) { return false; }
            doFront();
            bool const hasReacted = asSequential_coreReact();
            doBack();
            return hasReacted;
        }

        void asSequential_continue() { while (!m_current->asSubState_isFinal()) { if (!m_current->asSubState_continue()) { return; } } }

        void asSequential_reset();

        void asSequential_deepReset();

        void asSequential_suspend() { auto sequential = m_fstElem; do { sequential->asSubState_reset(); sequential = sequential->m_nxt; } while (sequential != nullptr); }

    protected:
        Sequential(InitMixee &init);
    };

    class Continueable
    {
        friend class Ground;
        template <class Ctxt> friend class HierarchicalState;

        Continueable() = default;
        Continueable(Continueable const &) = delete;
        Continueable(Continueable &&) = delete;
        Continueable& operator=(Continueable const &) = delete;
        Continueable& operator=(Continueable &&) = delete;

        virtual ~Continueable() = default;       

        virtual void injectContinue(ChainedEdge *_continue) = 0;
        virtual void fuseContinues()  = 0;
        virtual bool _continue() = 0;
    };

    class Ground : public SubState<Sequential>, public Continueable 
    {
        void asContextElem_fuse() final { SubState<Sequential>::asContextElem_fuse(); fuseContinues(); }
        bool asSubState_continue() final { return this->_continue(); }

    protected:
        Ground(internal::Sequential *ctxt) : SubState<Sequential>(ctxt) {}
    };

    class InitMixee : public internal::Ground 
    {
        bool asSubState_isInit() const final { return true; }

    public:
        InitMixee(internal::Sequential *ctxt) : internal::Ground(ctxt) {}
    };

    class FinalMixee : public internal::SubState<internal::Sequential> 
    {
        bool asSubState_isFinal() const final { return true; }

    public:
        FinalMixee(internal::Sequential *ctxt) : internal::SubState<internal::Sequential>(ctxt) {}
    };

    class BasicMixee : public internal::Ground
    {
    public:
        BasicMixee(internal::Sequential* ctxt) : internal::Ground(ctxt) {}
    };

    inline Sequential::Sequential(InitMixee &init) : m_init(init) {}
    
    inline void Sequential::asContext_fuse() {
        m_current = &m_init;
        Context<SubState<Sequential>>::asContext_fuse();
    }

    inline void Sequential::asSequential_exit() {
        onExit();
        m_current = &m_init;
    }

    inline void Sequential::asSequential_reset() {
        if (!m_current->asSubState_isInit()) {
            { SubState<Sequential> *subState = m_fstElem; do { subState->asSubState_deepReset(); subState = subState->m_nxt; } while (subState != nullptr); }
            onExit();
            m_current = &m_init;
        }
    }

    inline void Sequential::asSequential_deepReset() {
        if (!m_current->asSubState_isInit()) {
            { SubState<Sequential> *subState = m_fstElem; do { subState->asSubState_deepReset(); subState = subState->m_nxt; } while (subState != nullptr); }
            onExit();
            asScope_nextify();
            m_current = &m_init;
        }
    }

    template <class Ctxt>
    class HierarchicalState : public SubState<Ctxt>, public Continueable
    {
        friend class SuperMixee;
        friend class CompositeMixee;

        virtual void injectImmediatePreempt(ChainedEdge *immediatePreempt) = 0;
        virtual void fuseImmediatePreempts() = 0;
        virtual bool immediatePreempt() = 0;

        virtual void injectPreempt(ChainedEdge *preempt) = 0;
        virtual void fusePreempts() = 0;
        virtual bool preempt() = 0;

        void asContextElem_fuse() override {
            SubState<Ctxt>::asContextElem_fuse();
            fuseContinues();
            fuseImmediatePreempts();
            fusePreempts();
        }

        virtual bool asHierarchicalState_hasTerminated() const = 0;

        bool asSubState_continue() final { if (asHierarchicalState_hasTerminated()) { return _continue(); } else { return false; } }

        bool asHierarchicalState_immediatePreempt() { return immediatePreempt(); }

        bool asHierarchicalState_preempt() { return preempt(); }

    protected:
        HierarchicalState(Ctxt *ctxt) : SubState<Ctxt>(ctxt) {}
    };

    class Nextifier
    {
        friend class SuperMixee;
        friend class CompositeMixee;

        Scope *ctxt;
        Nextifier(Scope*_ctxt) : ctxt(_ctxt) {}
        ~Nextifier() { ctxt->asScope_nextify(); }
    };

    class SuperMixee : public internal::Sequential, public internal::HierarchicalState<internal::Sequential>
    {
        friend class Nextifier;

        void asContextElem_fuse() final { internal::HierarchicalState<internal::Sequential>::asContextElem_fuse(); asContext_fuse(); }

        bool asHierarchicalState_hasTerminated() const final { return asSequential_hasTerminated(); } 

        bool asSubState_react() final {
            if (asHierarchicalState_hasTerminated()) { return internal::SubState<internal::Sequential>::asSubState_react(); }

            internal::Nextifier nextify{this};

            if (asHierarchicalState_immediatePreempt()) { return true; }

            bool const hasReacted = asSequential_react();
            
            bool const hasPreempted = [&]{ if (asSequential_hasTerminated()) { return false; } else { return asHierarchicalState_preempt(); } }();

            return hasReacted || hasPreempted;
        }

        void asSubState_nextify() final { asScope_nextify(); }

        void asSource_exit() final { asSequential_exit(); }

        void asTarget_entry() final { asSequential_entry(); }

        void asSubState_abort() final { asSubState_reset(); }

        void asSubState_suspend() final { asSequential_suspend(); }

        void asSubState_reset() final { asSequential_reset(); }

        void asSubState_deepReset() final { asSequential_deepReset(); }

    public:
        SuperMixee(internal::Sequential *ctxt, InitMixee &init) : internal::Sequential(init), internal::HierarchicalState<internal::Sequential>(ctxt) {}
    };

    class CompositeMixee;

    class RegionMixee : public internal::Sequential, public internal::Chained<RegionMixee>
    {
        template <class CRTP> friend class internal::Chained;
        template <class Elem> friend class internal::Context;
        friend class CompositeMixee;

        CompositeMixee *const m_ctxt;

        RegionMixee *m_nxt = nullptr;
        void injectNxtElem(RegionMixee *nxtElem) { m_nxt = nxtElem; }

        void asContextElem_fuse() { asContext_fuse(); }

    public:
        RegionMixee(CompositeMixee *ctxt, InitMixee &init);
    };

    class CompositeMixee : public internal::Context<RegionMixee>, public internal::HierarchicalState<internal::Sequential>
    {
        friend class RegionMixee;

        void injectRegion(RegionMixee *region) { Context<RegionMixee>::injectElem(region); }

        void asContextElem_fuse() final { internal::HierarchicalState<internal::Sequential>::asContextElem_fuse(); asContext_fuse(); }

        bool asHierarchicalState_hasTerminated() const final {
            bool isFinal = true;
            { auto const *region = m_fstElem; do { isFinal &= region->asSequential_hasTerminated(); region = region->m_nxt; } while (region != nullptr); }
            return isFinal;
        }

        bool asSubState_react() final {
            if (asHierarchicalState_hasTerminated()) { return internal::SubState<internal::Sequential>::asSubState_react(); }

            internal::Nextifier nextify{this};

            if (asHierarchicalState_immediatePreempt()) { return true; };

            doFront();
            bool const hasReacted = [&]{
                bool hasReacted = false;
                { auto region = m_fstElem; do { hasReacted |= region->asSequential_react(); region = region->m_nxt; } while (region != nullptr); }
                return hasReacted;
            }();
            doBack();

            bool const hasPreempted = [&]{ if (asHierarchicalState_hasTerminated()) { return false; } else { return asHierarchicalState_preempt(); } }();

            return hasReacted || hasPreempted;
        }

        void asSubState_nextify() final {
            { auto region = m_fstElem; do { region->asScope_nextify(); region = region->m_nxt; } while (region != nullptr); }
            asScope_nextify();
        }

        void asSource_exit() final {
            { auto region = m_fstElem; do { region->asSequential_exit(); region = region->m_nxt; } while (region != nullptr); }
            onExit();
        }

        void asTarget_entry() final {
            onEntry();
            { auto region = m_fstElem; do { region->asSequential_entry(); region = region->m_nxt; } while (region != nullptr); }
        }

        void asSubState_abort() final { asSubState_reset(); }

        void asSubState_suspend() final {
            auto region = m_fstElem; do { region->asSequential_suspend(); region = region->m_nxt; } while (region != nullptr);
        }

        void asSubState_reset() final {
            { auto region = m_fstElem; do { region->asSequential_reset(); region = region->m_nxt; } while (region != nullptr); }
            onExit();
        }

        void asSubState_deepReset() final {
            { auto region = m_fstElem; do { region->asSequential_deepReset(); region = region->m_nxt; } while (region != nullptr); }
            onExit();
            asSubState_nextify();
        }

    public:
        CompositeMixee(internal::Sequential *ctxt) : internal::HierarchicalState<internal::Sequential>(ctxt) {}
    };

    inline RegionMixee::RegionMixee(CompositeMixee *ctxt, InitMixee &init) : internal::Sequential(init), m_ctxt(ctxt) {
        ZFSM_ERROR(ctxt == nullptr, "context pointer in region is null");
        m_ctxt->injectRegion(this); 
    }

    class TopMixee : public internal::Sequential 
    {
        bool isFused = false;
        bool isAtEntry = true;

    public:
        void fuse() { ZFSM_ERROR(isFused, "state machine fused more than once"); asContext_fuse(); isFused = true; }

        bool tick() {
            ZFSM_ERROR(!isFused, "state machine not fused");
            if (isAtEntry) { 
                onEntry(); 
                asSequential_coreReact();
                isAtEntry = false; 
            } else {
                asSequential_react();
            }
            asScope_nextify();
            bool const isFinal = asSequential_hasTerminated();
            return !isFinal;
        }

        template<typename... Raised>
        bool tick(internal::ChainedSignal::Raised, Raised... signals) { return tick(signals...); }

        void exit() { ZFSM_ERROR(!isFused, "state machine not fused"); asSequential_deepReset(); isAtEntry = true; }

    protected:
        TopMixee(InitMixee &init) : internal::Sequential(init) {}
    };

} // namespace internal

enum Capability { SIGNAL, ON_ENTRY, ON_EXIT, DO_FRONT, DO_BACK, REACT, CONTINUE, PREEMPT, IMMEDIATE_PREEMPT };

namespace internal
{
    template <bool> class SignalMixIn;

    template <>
    class SignalMixIn<false> {
        template <class Mixee, zfsm::Capability...> friend class ScopeMix;

        void inject(ChainedSignal *) { ZFSM_ERROR(true, "missing SIGNAL capability"); }
        void nextify() const {}
    };

    template <>
    class SignalMixIn<true> {
        template <class Mixee, zfsm::Capability...> friend class ScopeMix;

        ChainedSignal *m_fstSignal = nullptr;

        void inject(ChainedSignal *signal) { signal->chain(m_fstSignal); m_fstSignal = signal; }
        void nextify() const { 
            ZFSM_ERROR(m_fstSignal == nullptr, "missing signal(s)");
            { auto signal = m_fstSignal; do { signal->reset(); signal = signal->m_nxt; } while (signal != nullptr); }
        }
    };

    template <ContextActionType, bool> class ContextActionMixIn;

    template <ContextActionType type>
    class ContextActionMixIn<type, false> {
        template <class Mixee, zfsm::Capability...> friend class ScopeMix;

        void inject(ChainedContextAction *) { ZFSM_ERROR(true, "missing state action capability"); }
        void reverseActionChain() {}
        void enact() const {}
    };

    template <ContextActionType type>
    class ContextActionMixIn<type, true> {
        template <class Mixee, zfsm::Capability...> friend class ScopeMix;

        ChainedContextAction *m_fst = nullptr;

        void inject(ChainedContextAction *action) { action->chain(m_fst); m_fst = action; }
        void reverseActionChain() { ZFSM_ERROR(m_fst == nullptr, "missing state action(s)"); m_fst = m_fst->reverseElemChain(); }
        void enact() const { auto action = m_fst; do { action->chainedAction_evalExec(); action = action->m_nxt; } while (action != nullptr); }
    };

    template <ArrowType, bool> class ArrowMixIn;

    template <ArrowType type>
    class ArrowMixIn<type, false> {
        template <class Mixee, zfsm::Capability...> friend class SubStateMix;
        template <class Mixee, zfsm::Capability...> friend class GroundMix;
        template <class Mixee, zfsm::Capability...> friend class HierarchicalStateMix;

        void inject(ChainedEdge *) { ZFSM_ERROR(true, "missing arrow capability"); }
        void reverseArrowChain() {}
        bool enact(bool) const { return false; }
    };

    template <ArrowType type>
    class ArrowMixIn<type, true> {
        template <class Mixee, zfsm::Capability...> friend class SubStateMix;
        template <class Mixee, zfsm::Capability...> friend class GroundMix;
        template <class Mixee, zfsm::Capability...> friend class HierarchicalStateMix;

        ChainedEdge *m_fst = nullptr;

        void inject(ChainedEdge *arrow) { arrow->chain(m_fst); m_fst = arrow; }
        void reverseArrowChain() { ZFSM_ERROR(m_fst == nullptr, "missing arrow(s)"); m_fst = m_fst->reverseElemChain(); }
        bool enact(bool immediatePreempt) const {
            { auto *arrow = m_fst; do { if (arrow->chainedArrow_evalExec(immediatePreempt)) { return true; } arrow = arrow->m_nxt; } while (arrow != nullptr); }
            return false;
        }
    };

    template <Capability required, Capability... capability> 
    struct isPresent { 
        static constexpr bool value = ((required == capability) || ...); 
    };

    template <class Mixee, Capability... capability>
    class ScopeMix :
        public Mixee,
        public SignalMixIn<isPresent<SIGNAL, capability...>::value>,
        public ContextActionMixIn<ContextActionType::ON_ENTRY, isPresent<ON_ENTRY, capability...>::value>,
        public ContextActionMixIn<ContextActionType::ON_EXIT, isPresent<ON_EXIT, capability...>::value>,
        public ContextActionMixIn<ContextActionType::DO_FRONT, isPresent<DO_FRONT, capability...>::value>,
        public ContextActionMixIn<ContextActionType::DO_BACK, isPresent<DO_BACK, capability...>::value>
    {
        using _SignalMixIn = SignalMixIn<isPresent<SIGNAL, capability...>::value>;
        using EntryMixIn = ContextActionMixIn<ContextActionType::ON_ENTRY, isPresent<ON_ENTRY, capability...>::value>;
        using ExitMixIn = ContextActionMixIn<ContextActionType::ON_EXIT, isPresent<ON_EXIT, capability...>::value>;
        using DoFrontMixIn = ContextActionMixIn<ContextActionType::DO_FRONT, isPresent<DO_FRONT, capability...>::value>;
        using DoBackMixIn = ContextActionMixIn<ContextActionType::DO_BACK, isPresent<DO_BACK, capability...>::value>;

        void injectSignal(ChainedSignal *signal) final { _SignalMixIn::inject(signal); }
        void asScope_nextify() const final { _SignalMixIn::nextify(); }

        void injectEntry(ChainedOnEntry *onEntry) final { EntryMixIn::inject(onEntry); }
        void fuseEntries() final { EntryMixIn::reverseActionChain(); }
        void onEntry() const final { EntryMixIn::enact(); }

        void injectExit(ChainedOnExit *onExit) final { ExitMixIn::inject(onExit); }
        void fuseExits() final { ExitMixIn::reverseActionChain(); }
        void onExit() const final { ExitMixIn::enact(); }

        void injectDoFront(ChainedDoFront *doFront) final { DoFrontMixIn::inject(doFront); }
        void fuseDoFronts() final { DoFrontMixIn::reverseActionChain(); }
        void doFront() const final { DoFrontMixIn::enact(); }

        void injectDoBack(ChainedDoBack *doBack) final { DoBackMixIn::inject(doBack); }
        void fuseDoBacks() final{ DoBackMixIn::reverseActionChain(); }
        void doBack() const final { DoBackMixIn::enact(); }

    protected:
        using Mixee::Mixee;
    };

    template <class Mixee, Capability... capability>
    class SubStateMix : public Mixee, public ArrowMixIn<ArrowType::REACT, isPresent<REACT, capability...>::value>
    {
        friend class InjectReact;

        using ReactMixIn = ArrowMixIn<ArrowType::REACT, isPresent<REACT, capability...>::value>;

        void injectReact(ChainedEdge *react) final { ReactMixIn::inject(react); }
        void fuseReacts() final { ReactMixIn::reverseArrowChain(); }
        bool react() final { return ReactMixIn::enact(false); }

    protected:
        using Mixee::Mixee;
    };

    template <class Mixee, Capability... capability>
    class GroundMix : public Mixee, public ArrowMixIn<ArrowType::CONTINUE, isPresent<CONTINUE, capability...>::value>
    {
        friend class InjectContinue;

        using ContinueMixIn = ArrowMixIn<ArrowType::CONTINUE, isPresent<CONTINUE, capability...>::value>;

        void injectContinue(ChainedEdge *_continue) final { ContinueMixIn::inject(_continue); }
        void fuseContinues() final { ContinueMixIn::reverseArrowChain(); }
        bool _continue() final { return ContinueMixIn::enact(false); }

    protected:
        using Mixee::Mixee;
    };

    template <class Mixee, Capability... capability>
    class HierarchicalStateMix :
        public Mixee,
        public ArrowMixIn<ArrowType::CONTINUE, isPresent<CONTINUE, capability...>::value>,
        public ArrowMixIn<ArrowType::PREEMPT, isPresent<PREEMPT, capability...>::value>,
        public ArrowMixIn<ArrowType::IMMEDIATE_PREEMPT, isPresent<IMMEDIATE_PREEMPT, capability...>::value>
    {
        friend class InjectContinue;
        friend class InjectPreempt;
        friend class InjectImmediatePreempt;
        template <class T, class OnLeave> friend class Preempt;
        template <class T, class OnLeave> friend class ImmediatePreempt;

        using ContinueMixIn = ArrowMixIn<ArrowType::CONTINUE, isPresent<CONTINUE, capability...>::value>;
        using PreemptMixIn = ArrowMixIn<ArrowType::PREEMPT, isPresent<PREEMPT, capability...>::value>;
        using ImmediatePreemptMixIn = ArrowMixIn<ArrowType::IMMEDIATE_PREEMPT, isPresent<IMMEDIATE_PREEMPT, capability...>::value>;

        void injectContinue(ChainedEdge *_continue) final { ContinueMixIn::inject(_continue); }
        void fuseContinues() final { ContinueMixIn::reverseArrowChain(); }
        bool _continue() final { return ContinueMixIn::enact(false); }

        void injectPreempt(ChainedEdge *preempt) final { PreemptMixIn::inject(preempt); }
        void fusePreempts() final { PreemptMixIn::reverseArrowChain(); }
        bool preempt() final { return PreemptMixIn::enact(false); }

        void injectImmediatePreempt(ChainedEdge *immediatePreempt) final { ImmediatePreemptMixIn::inject(immediatePreempt); }
        void fuseImmediatePreempts() final { ImmediatePreemptMixIn::reverseArrowChain(); }
        bool immediatePreempt() final { return ImmediatePreemptMixIn::enact(true); }

    protected:
        using Mixee::Mixee;
    };

} // namespace internal

template <Capability... capability> using Init =      internal::GroundMix<internal::SubStateMix<zfsm::internal::InitMixee, capability...>, capability...>;
                                    using Final =     internal::SubStateMix<zfsm::internal::FinalMixee>;
template <Capability... capability> using Basic =     internal::GroundMix<internal::SubStateMix<zfsm::internal::BasicMixee, capability...>, capability...>;
template <Capability... capability> using Super =     internal::HierarchicalStateMix<internal::SubStateMix<internal::ScopeMix<zfsm::internal::SuperMixee, capability...>, capability...>, capability...>;
template <Capability... capability> using Region =    internal::ScopeMix<zfsm::internal::RegionMixee, capability...>;
template <Capability... capability> using Composite = internal::HierarchicalStateMix<internal::SubStateMix<internal::ScopeMix<zfsm::internal::CompositeMixee, capability...>, capability...>, capability...>;
template <Capability... capability> using Top =       internal::ScopeMix<zfsm::internal::TopMixee, capability...>;

namespace internal
{
    class Edge : public ChainedEdge 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        SubState<Sequential> &m_from;
        SubState<Sequential> &m_to;

        Edge(Ground &from, BasicMixee &to) : m_from(from), m_to(to) { 
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from ground to basic state"); 
        }

        template <class Ctxt>
        Edge(Ground &from, HierarchicalState<Ctxt> &to) : m_from(from), m_to(to) { 
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from ground to hierarchical state"); 
        }

        Edge(Ground &from, FinalMixee &to) : m_from(from), m_to(to) { 
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from ground to final state"); 
        }

        template <class Ctxt>
        Edge(HierarchicalState<Ctxt> &from, BasicMixee &to) : m_from(from), m_to(to) { 
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from hierarchical to basic state");
        }

        template <class Ctxt>
        Edge(HierarchicalState<Ctxt> &from, HierarchicalState<Ctxt> &to) : m_from(from), m_to(to) {
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from hierarchical to hierarchical state");
        }

        template <class Ctxt>
        Edge(HierarchicalState<Ctxt> &from, FinalMixee &to) : m_from(from), m_to(to) {
            ZFSM_ERROR(m_from.m_ctxt != m_to.m_ctxt, "context mismatch on arrow from hierarchical to final state"); 
        }
    };

    struct Void {};

    template <typename T, class Guard, class SFINAE = void> class GuardDecoration;

    template <typename T, class Guard>
    class GuardDecoration<T, Guard, typename std::enable_if<std::is_void<T>::value && std::is_same<Guard, Void>::value>::type> 
    {
        template <class Inject, class OnLeave, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedContextAction;  

        inline bool evaluateGuard() { return true; }
    };

    template <typename T, class Guard>
    class GuardDecoration<T, Guard, typename std::enable_if<std::is_void<T>::value && std::is_invocable_r<bool, Guard>::value>::type> 
    {
        template <class Inject, class OnLeave, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedContextAction;  
        
        Guard m_guard;

        inline bool evaluateGuard() { return m_guard(); }

    protected:
        GuardDecoration(Guard guard) : m_guard(std::move(guard)) {}
    };

    template <typename T, class Guard>
    class GuardDecoration<T, Guard, typename std::enable_if<(!std::is_void<T>::value) && std::is_invocable<Guard>::value>::type> 
    {
        template <class Inject, class OnLeave, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class _Guard, class Action, class SFINAE> friend class DecoratedContextAction;  
        
        Guard m_guard;

        inline std::optional<T> evaluateGuard() { return m_guard(); }

    protected:
        GuardDecoration(Guard guard) : m_guard(std::move(guard)) {}
    };

    template <typename T, class Action, class SFINAE = void> class ActionDecoration;

    template <typename T, class Action>
    class ActionDecoration<T, Action, typename std::enable_if<std::is_void<T>::value && std::is_same<Action, Void>::value>::type> 
    {
        template <class Inject, typename _T, class OnLeave, class Guard, class _Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  
        
        inline void execAction() {}
    };

    template <typename T, class Action>
    class ActionDecoration<T, Action, typename std::enable_if<std::is_void<T>::value && std::is_invocable<Action>::value>::type> 
    {
        template <class Inject, typename _T, class OnLeave, class Guard, class _Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  
        
        Action m_action;

        inline void execAction() { m_action(); }

    protected:
        ActionDecoration(Action action) : m_action(std::move(action)) {}
    };

    template <typename T, class Action>
    class ActionDecoration<T, Action, typename std::enable_if<(!std::is_void<T>::value) && std::is_invocable<Action, T>::value>::type> 
    {
        template <class Inject, typename _T, class OnLeave, class Guard, class _Action, class SFINAE> friend class DecoratedEdge;
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  
        
        Action m_action;

        inline void execAction(T arg) { m_action(std::move(arg)); }

    protected:
        ActionDecoration(Action action) : m_action(std::move(action)) {}
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE = void> class DecoratedEdge;

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class DecoratedEdge<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value>::type> : 
        public Edge,
        public GuardDecoration<void, Guard>,
        public ActionDecoration<void, Action>
    {
        bool chainedArrow_evalExec(bool immediatePreempt) final {
            if (!this->evaluateGuard()) { return false; }

            OnLeave::doLeave(m_from);
            if (!immediatePreempt) { m_from.asSubState_nextify(); }
            this->execAction();
            m_to.m_ctxt->updateCurrent(&m_to);
            m_to.asTarget_entry();
            m_to.asSubState_nextify();
            return true;
        } 

    protected:
        template <class From, class To>
        DecoratedEdge(From &from, To &to)
            : Edge(from, to) {
            Inject::doInject(from, this);
        }

        template <class From, class To>
        DecoratedEdge(From &from, Guard guard, To &to)
            : Edge(from, to), GuardDecoration<void, Guard>(std::move(guard)) {
            Inject::doInject(from, this);
        }

        template <class From, class To>
        DecoratedEdge(From &from, To &to, Action action)
            : Edge(from, to), ActionDecoration<void, Action>(std::move(action)) {
            Inject::doInject(from, this);
        }

        template <class From, class To>
        DecoratedEdge(From &from, Guard guard, To &to, Action action)
            : Edge(from, to), GuardDecoration<void, Guard>(std::move(guard)), ActionDecoration<void, Action>(std::move(action)) {
            Inject::doInject(from, this);
        }
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class DecoratedEdge<Inject, OnLeave, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value>::type> : 
        public Edge,
        public GuardDecoration<T, Guard>,
        public ActionDecoration<T, Action>
    {
        bool chainedArrow_evalExec(bool immediatePreempt) final {
            std::optional<T> carry = this->evaluateGuard();
            if (!carry.has_value()) { return false; }

            OnLeave::doLeave(m_from);
            if (!immediatePreempt) { m_from.asSubState_nextify(); }
            this->execAction(std::move(carry.value()));
            m_to.m_ctxt->updateCurrent(&m_to);
            m_to.asTarget_entry();
            m_to.asSubState_nextify();
            return true;
        }

    protected:
        template <class From, class To>
        DecoratedEdge(From &from, Guard guard, To &to, Action action)
            : Edge(from, to), GuardDecoration<T, Guard>(std::move(guard)), ActionDecoration<T, Action>(std::move(action)) {
            Inject::doInject(from, this);
        }
    };
        
} // namespace internal

struct Std {};
struct Nil {};

namespace internal
{
    template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE = void> class Arrow;

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Nil>::value && std::is_same<Action, Nil>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Void, Void>
    {
    public:
        template <class From, class To>
        Arrow(From &from, To &to)
            : DecoratedEdge<Inject, OnLeave, void, Void, Void>(from, to) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_same<Action, Nil>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Void>
    {
    public:
        template <class From, class To>
        Arrow(From &from, std::function<bool()> guard, To &to)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Void>(from, std::move(guard), to) {}      

        template <class From, class To, typename _T>
        Arrow(From &from, Signal<_T> &signal, To &to)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Void>(from, [&]{ return signal(); }, to) {}                          
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Nil>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Void, std::function<void()>>
    {
    public:
        template <class From, class To>
        Arrow(From &from, To &to, std::function<void()> action)
            : DecoratedEdge<Inject, OnLeave, void, Void, std::function<void()>>(from, to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>
    {
    public:
        template <class From, class To>
        Arrow(From &from, To &to)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, []{ return true; }, to, []{}) {}      

        template <class From, class To>
        Arrow(From &from, std::function<bool()> guard, To &to)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, std::move(guard), to, []{}) {}      

        template <class From, class To, typename _T>
        Arrow(From &from, Signal<_T> &signal, To &to)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, [&]{ return signal(); }, to, []{}) {}      

        template <class From, class To>
        Arrow(From &from, To &to, std::function<void()> action)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, []{ return true; }, to, std::move(action)) {}      

        template <class From, class To>
        Arrow(From &from, std::function<bool()> guard, To &to, std::function<void()> action)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, std::move(guard), to, std::move(action)) {}      

        template <class From, class To, typename _T>
        Arrow(From &from, Signal<_T> &signal, To &to, std::function<void()> action)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, std::function<void()>>(from, [&]{ return signal(); }, to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_same<Action, Nil>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Guard, Void>
    {
    public:
        template <class From, class To>
        Arrow(From &from, Guard guard, To &to)
            : DecoratedEdge<Inject, OnLeave, void, Guard, Void>(from, std::move(guard), to) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Nil>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Void, Action>
    {
    public:
        template <class From, class To>
        Arrow(From &from, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, void, Void, Action>(from, to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Guard, Action>
    {
    public:
        template <class From, class To>
        Arrow(From &from, Guard guard, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, void, Guard, Action>(from, std::move(guard), to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, Guard, std::function<void()>>
    {
    public:
        template <class From, class To>
        Arrow(From &from, Guard guard, To &to, std::function<void()> action)
            : DecoratedEdge<Inject, OnLeave, void, Guard, std::function<void()>>(from, std::move(guard), to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Action>
    {
    public:
        template <class From, class To>
        Arrow(From &from, std::function<bool()> guard, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Action>(from, std::move(guard), to, std::move(action)) {}      

        template <class From, class To, typename _T>
        Arrow(From &from, Signal<_T> &signal, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, void, std::function<bool()>, Action>(from, [&]{ return signal(); }, to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, std::function<void(T)>>
    {
    public:
        template <class From, class To>
        Arrow(From &from, std::function<std::optional<T>()> guard, To &to, std::function<void(T)> action)
            : DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, std::function<void(T)>>(from, std::move(guard), to, std::move(action)) {}      

        template <class From, class To>
        Arrow(From &from, Signal<T> &signal, To &to, std::function<void(T)> action)
            : DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, std::function<void(T)>>(
                from, [&]()->std::optional<T>{ if (signal()) { return signal.getValue(); } else { return std::nullopt; } }, to, std::move(action)
            ) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_invocable<Action, T>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, T, Guard, Action>
    {
    public:
        template <class From, class To>
        Arrow(From &from, Guard guard, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, T, Guard, Action>(from, std::move(guard), to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, T, Guard, std::function<void(T)>>
    {
    public:
        template <class From, class To>
        Arrow(From &from, Guard guard, To &to, std::function<void(T)> action)
            : DecoratedEdge<Inject, OnLeave, T, Guard, std::function<void(T)>>(from, std::move(guard), to, std::move(action)) {}      
    };

    template <class Inject, class OnLeave, typename T, class Guard, class Action>
    class Arrow<Inject, OnLeave, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_invocable<Action, T>::value>::type> 
        : public DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, Action>
    {
    public:
        template <class From, class To>
        Arrow(From &from, std::function<std::optional<T>()> guard, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, Action>(from, std::move(guard), to, std::move(action)) {}      

        template <class From, class To>
        Arrow(From &from, Signal<T> &signal, To &to, Action action)
            : DecoratedEdge<Inject, OnLeave, T, std::function<std::optional<T>()>, Action>(
                from, [&]()->std::optional<T>{ if (signal()) { return signal.getValue(); } else { return std::nullopt; } }, to, std::move(action)
            ) {}      
    };

    class OnLeaveByMove 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        static void doLeave(SubState<Sequential> &from) { from.asSource_exit(); }
    };

    class InjectReact 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        template <class From>
        static void doInject(From &from, Edge *react) { from.injectReact(react); }
    };

    class InjectContinue 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        template <class From>
        static void doInject(From &from, Edge *_continue) { from.injectContinue(_continue); }
    };

} // namespace internal

template <typename T = void, class Guard = Std, class Action = Std> 
using React = internal::Arrow<internal::InjectReact, internal::OnLeaveByMove, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std>
using Continue = internal::Arrow<internal::InjectContinue, internal::OnLeaveByMove, T, Guard, Action>;

namespace internal
{
    class InjectPreempt
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        template <class From>
        static void doInject(From &from, Edge *preempt) { from.injectPreempt(preempt); }
    };

    class InjectImmediatePreempt 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        template <class From>
        static void doInject(From &from, Edge *immediatePreempt) { from.injectImmediatePreempt(immediatePreempt); }
    };

    class OnLeaveByAbort 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        static void doLeave(SubState<Sequential> &from) { from.asSubState_abort(); }
    };

    class OnLeaveBySuspend 
    {
        template <class Inject, class OnLeave, typename T, class Guard, class Action, class SFINAE> friend class DecoratedEdge;

        static void doLeave(SubState<Sequential> &) {}
    };

} // namespace internal

template <typename T = void, class Guard = Std, class Action = Std> 
using Abort = internal::Arrow<internal::InjectPreempt, internal::OnLeaveByAbort, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std>
using ImmediateAbort = internal::Arrow<internal::InjectImmediatePreempt, internal::OnLeaveByAbort, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std> 
using Suspend = internal::Arrow<internal::InjectPreempt, internal::OnLeaveBySuspend, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std>
using ImmediateSuspend = internal::Arrow<internal::InjectImmediatePreempt, internal::OnLeaveBySuspend, T, Guard, Action>;

namespace internal
{
    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action, class SFINAE = void> class DecoratedContextAction;

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class DecoratedContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value>::type> :
        public ChainedContextAction,
        public GuardDecoration<void, Guard>,
        public ActionDecoration<void, Action>
    {
        void chainedAction_evalExec() final { if (this->evaluateGuard()) { this->execAction(); } }

    public:
        DecoratedContextAction(Scope *scope, Guard guard)
            : GuardDecoration<void, Guard>(std::move(guard)) { 
            Inject::doInject(scope, this); 
        }

        DecoratedContextAction(Scope *scope, Action action)
            : ActionDecoration<void, Action>(std::move(action)) { 
            Inject::doInject(scope, this); 
        }
        
        DecoratedContextAction(Scope *scope, Guard guard, Action action)
            : GuardDecoration<void, Guard>(std::move(guard)), ActionDecoration<void, Action>(std::move(action)) { 
            Inject::doInject(scope, this);
        }
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class DecoratedContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value>::type> :
        public ChainedContextAction,
        public GuardDecoration<T, Guard>,
        public ActionDecoration<T, Action>
    {
        void chainedAction_evalExec() final { 
            std::optional<T> carry = this->evaluateGuard(); 
            if (carry.has_value()) { this->execAction(std::move(*carry)); }
        }

    public:
        DecoratedContextAction(Scope *scope, Guard guard, Action action)
            : GuardDecoration<T, Guard>(std::move(guard)), ActionDecoration<T, Action>(std::move(action)) { 
            Inject::doInject(scope, this);
        }
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action, class SFINAE = void> class ContextAction;  
    
    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Nil>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, Void, std::function<void()>>
    {
    public:
        ContextAction(Scope *scope, std::function<void()> action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, Void, std::function<void()>>(scope, std::move(action)) {}      
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Nil>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, Void, Action>
    {
    public:
        ContextAction(Scope *scope, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, Void, Action>(scope, std::move(action)) {}
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, std::function<void()>>
    {
    public:
        ContextAction(Scope *scope, std::function<bool()> guard, std::function<void()> action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, std::function<void()>>(scope, std::move(guard), std::move(action)) {}

        ContextAction(Scope *scope, std::function<void()> action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, std::function<void()>>(scope, []{ return true; }, std::move(action)) {}

        template<typename _T>
        ContextAction(Scope *scope, Signal<_T> &signal, std::function<void()> action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, std::function<void()>>(scope, [&]{ return signal(); }, std::move(action)) {}                
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, Guard, std::function<void()>>
    {
    public:
        ContextAction(Scope *scope, Guard guard, std::function<void()> action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, Guard, std::function<void()>>(scope, std::move(guard), std::move(action)) {}      
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, Action>
    {
    public:
        ContextAction(Scope *scope, std::function<bool()> guard, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, Action>(scope, std::move(guard), std::move(action)) {}     
            
        template<typename _T>
        ContextAction(Scope *scope, Signal<_T> &signal, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, std::function<bool()>, Action>(scope, [&]{ return signal(); }, std::move(action)) {}     
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_invocable<Action>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, void, Guard, Action>
    {
    public:
        ContextAction(Scope *scope, Guard guard, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, void, Guard, Action>(scope, std::move(guard), std::move(action)) {}      
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, std::function<void(T)>>
    {
    public:
        ContextAction(Scope *scope, std::function<std::optional<T>()> guard, std::function<void(T)> action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, std::function<void(T)>>(scope, std::move(guard), std::move(action)) {}

        ContextAction(Scope *scope, Signal<T> &signal, std::function<void(T)> action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, std::function<void(T)>>(
                scope, [&]()->std::optional<T>{ if (signal()) { return signal.getValue(); } else { return std::nullopt; } }, std::move(action)
            ) {}                
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_same<Action, Std>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, T, Guard, std::function<void(T)>>
    {
    public:
        ContextAction(Scope *scope, Guard guard, std::function<void(T)> action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, Guard, std::function<void(T)>>(scope, std::move(guard), std::move(action)) {}      
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_same<Guard, Std>::value && std::is_invocable<Action, T>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, Action>
    {
    public:
        ContextAction(Scope *scope, std::function<std::optional<T>()> guard, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, Action>(scope, std::move(guard), std::move(action)) {}      

        ContextAction(Scope *scope, Signal<T> &signal, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, std::function<std::optional<T>()>, Action>(
                scope, [&]()->std::optional<T>{ if (signal()) { return signal.getValue(); } else { return std::nullopt; } }, std::move(action)
            ) {}      
    };

    template <class ChainedContextAction, class Inject, typename T, class Guard, class Action>
    class ContextAction<ChainedContextAction, Inject, T, Guard, Action, typename std::enable_if<!std::is_void<T>::value &&
        std::is_invocable<Guard>::value && std::is_invocable<Action, T>::value>::type> 
        : public DecoratedContextAction<ChainedContextAction, Inject, T, Guard, Action>
    {
    public:
        ContextAction(Scope *scope, Guard guard, Action action)
            : DecoratedContextAction<ChainedContextAction, Inject, T, Guard, Action>(scope, std::move(guard), std::move(action)) {}      
    };

    class InjectOnEntry 
    { 
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  
 
        static void doInject(Scope *scope, ChainedOnEntry *onEntry) { 
            ZFSM_ERROR(scope == nullptr, "scope pointer on entry action is null"); 
            scope->injectEntry(onEntry); 
        } 
    };

    class InjectOnExit
    { 
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  

        static void doInject(Scope *scope, ChainedOnExit *onExit) {
            ZFSM_ERROR(scope == nullptr, "scope pointer on exit action is null");
            scope->injectExit(onExit);
        } 
    };

    class InjectDoFront
    { 
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  

        static void doInject(Scope *scope, ChainedDoFront *doFront) {
            ZFSM_ERROR(scope == nullptr, "scope pointer on do-front action is null");
            scope->injectDoFront(doFront);
        } 
    };
    
    class InjectDoBack
    { 
        template <class ChainedContextAction, class Inject, typename _T, class Guard, class _Action, class SFINAE> friend class DecoratedContextAction;  

        static void doInject(Scope *scope, ChainedDoBack *doBack) {
            ZFSM_ERROR(scope == nullptr, "scope pointer on do-back action null");
            scope->injectDoBack(doBack);
        }
    };

} // namespace internal

template <typename T = void, class Guard = Std, class Action = Std> 
using OnEntry = internal::ContextAction<internal::ChainedOnEntry, internal::InjectOnEntry, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std> 
using OnExit = internal::ContextAction<internal::ChainedOnExit, internal::InjectOnExit, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std> 
using DoFront = internal::ContextAction<internal::ChainedDoFront, internal::InjectDoFront, T, Guard, Action>;

template <typename T = void, class Guard = Std, class Action = Std> 
using DoBack = internal::ContextAction<internal::ChainedDoBack, internal::InjectDoBack, T, Guard, Action>;

} // namespace zfsm

#undef ZFSM_ERROR

#endif // INCLUDED_ZFSM_HPP
