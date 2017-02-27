#pragma once

inline std::string message_text(
    const Evaluation &evaluation,
    std::string &&text,
    size_t index) {

    return text;
}

template<typename... Args>
std::string message_text(
    const Evaluation &evaluation,
    std::string &&text,
    size_t index,
    const BaseExpressionRef &arg,
    Args... args) {

    std::string new_text(text);
    const std::string placeholder(message_placeholder(index));

    const auto pos = new_text.find(placeholder);
    if (pos != std::string::npos) {
        new_text = new_text.replace(pos, placeholder.length(), evaluation.format_output(arg));
    }

    return message_text(evaluation, std::move(new_text), index + 1, args...);
}

template<typename... Args>
void Evaluation::message(const SymbolRef &name, const char *tag, const Args&... args) const {
    const auto &symbols = definitions.symbols();

    const BaseExpressionRef tag_str = String::construct(std::string(tag));

    const ExpressionRef message = expression(
        symbols.MessageName, name, tag_str);
    UnsafeStringRef text_template = static_cast<const Symbol*>(
        name.get())->lookup_message(message.get(), *this);

    if (!text_template) {
        const ExpressionRef general_message = expression(
            symbols.MessageName, symbols.General, tag_str);

        text_template = symbols.General->lookup_message(general_message.get(), *this);
    }

    if (text_template) {
        std::string text(text_template->utf8());
        std::unique_lock<std::mutex> lock(m_output_mutex);
        m_output->write(name->short_name(), tag, message_text(*this, std::move(text), 1, args...));
    }
}