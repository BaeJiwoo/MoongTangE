#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace MoongTangE::test {

class TcpTestListener;

class TcpTestSocket
{
  public:
    ~TcpTestSocket();
    TcpTestSocket(TcpTestSocket&&) noexcept;
    TcpTestSocket& operator=(TcpTestSocket&&) noexcept;

    TcpTestSocket(const TcpTestSocket&) = delete;
    TcpTestSocket& operator=(const TcpTestSocket&) = delete;

    static TcpTestSocket Connect(std::string_view address, std::uint16_t port);

    void ReadExact(std::span<std::uint8_t> destination);
    void Write(std::span<const std::uint8_t> source);
    void WriteByte(std::uint8_t value);
    [[nodiscard]] bool Closed() const;

  private:
    struct Impl;

    explicit TcpTestSocket(std::unique_ptr<Impl> implementation);

    std::unique_ptr<Impl> m_impl;

    friend class TcpTestListener;
};

class TcpTestListener
{
  public:
    TcpTestListener();
    ~TcpTestListener();
    TcpTestListener(TcpTestListener&&) noexcept;
    TcpTestListener& operator=(TcpTestListener&&) noexcept;

    TcpTestListener(const TcpTestListener&) = delete;
    TcpTestListener& operator=(const TcpTestListener&) = delete;

    [[nodiscard]] std::uint16_t Port() const;
    [[nodiscard]] TcpTestSocket Accept() const;
    void Close();

  private:
    struct Impl;

    std::unique_ptr<Impl> m_impl;
};

} // namespace MoongTangE::test
