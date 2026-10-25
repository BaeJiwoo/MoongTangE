#include <asio/config.hpp>

#if defined(MOONGTANGE_EXPECT_ASIO_IOCP)
#if !defined(ASIO_HAS_IOCP)
#error "MoongTangE expected Asio to use IOCP on Windows"
#endif
#elif defined(MOONGTANGE_EXPECT_ASIO_EPOLL)
#if !defined(ASIO_HAS_EPOLL)
#error "MoongTangE expected Asio to use epoll on Linux"
#endif
#elif defined(MOONGTANGE_EXPECT_ASIO_KQUEUE)
#if !defined(ASIO_HAS_KQUEUE)
#error "MoongTangE expected Asio to use kqueue on macOS"
#endif
#else
#error "MoongTangE Asio backend expectation was not configured"
#endif
