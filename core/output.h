#pragma once

class Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) = 0;
};

using OutputRef = std::shared_ptr<Output>;

class DefaultOutput : public Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {

        std::cout << name << "::" << tag << ": " << s << std::endl;
    }
};

class NoOutput : public Output {
public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {
    }
};

class TestOutput : public Output {
private:
    std::list<std::string> m_output;

public:
    virtual void write(
        const char *name,
        const char *tag,
        std::string &&s) {

        // ignore name and tag
        m_output.emplace_back(s);
    }

    void clear() {
        m_output.clear();
    }

    bool empty() {
        return m_output.empty();
    }

    bool test_empty();

    bool test_line(const std::string &expected, bool fail_expected);
};
