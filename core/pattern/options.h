#pragma once

class FastLeafSequence;
class SlowLeafSequence;

class OptionsProcessor : public PoolShared<OptionsProcessor> {
public:
    using MatchRest = std::function<index_t(index_t begin, index_t t, index_t end)>;

    enum MatcherType {
        Static,
        Dynamic
    };

    const MatcherType type;

protected:
    template<typename Sequence, typename Assign, typename Rollback>
    index_t parse(
        const Sequence &sequence,
        index_t begin,
        index_t end,
        const Assign &assign,
        const Rollback &rollback,
        const MatchRest &rest);

public:
    inline OptionsProcessor(MatcherType t) : type(t) {
    }

    virtual inline ~OptionsProcessor() {
    }

    virtual void reset() = 0;

    virtual index_t match(
        const SlowLeafSequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest) = 0;

    virtual index_t match(
        const FastLeafSequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest) = 0;
};

/*class EmptyOptionsProcessor : public OptionsProcessor {
public:
	virtual index_t match(
		const SlowLeafSequence &sequence,
		index_t begin,
		index_t end,
		const MatchRest &rest) {

		return parse(sequence, begin, end, [] (SymbolPtr, const BaseExpressionRef&) {}, [] () {}, rest);
	}

	virtual index_t match(
		const FastLeafSequence &sequence,
		index_t begin,
		index_t end,
		const MatchRest &rest) {

		return parse(sequence, begin, end, [] (SymbolPtr, const BaseExpressionRef&) {}, [] () {}, rest);
	}
};*/

class DynamicOptionsProcessor : public OptionsProcessor {
private:
    OptionsMap m_options;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest);

public:
    inline DynamicOptionsProcessor();

    virtual inline void reset() final {
        m_options.clear();
    }

    virtual index_t match(
            const SlowLeafSequence &sequence,
            index_t begin,
            index_t end,
            const MatchRest &rest) final;

    virtual index_t match(
            const FastLeafSequence &sequence,
            index_t begin,
            index_t end,
            const MatchRest &rest) final;

    inline const OptionsMap &options() const {
        return m_options;
    }
};

template<typename Options>
class OptionsDefinitions;

template<typename Options>
class StaticOptionsProcessor : public OptionsProcessor {
private:
    const Options *m_options_ptr;
    Options m_options;
    const OptionsDefinitions<Options> m_controller;

    template<typename Sequence>
    inline index_t do_match(
        const Sequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest);

public:
    inline StaticOptionsProcessor(const OptionsDefinitions<Options> &controller) :
        OptionsProcessor(Static), m_controller(controller) {

        m_options_ptr = &m_controller.defaults();
    }

    virtual inline void reset() final {
        m_options_ptr = &m_controller.defaults();
    }

    virtual index_t match(
        const SlowLeafSequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest) final;

    virtual index_t match(
        const FastLeafSequence &sequence,
        index_t begin,
        index_t end,
        const MatchRest &rest) final;

    inline const Options &options() const {
        return *m_options_ptr;
    }
};

using OptionsProcessorRef = ConstSharedPtr<OptionsProcessor>;
using UnsafeOptionsProcessorRef = UnsafeSharedPtr<OptionsProcessor>;
