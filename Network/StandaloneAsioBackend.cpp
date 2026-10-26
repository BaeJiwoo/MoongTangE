#include <asio/config.hpp>

#if defined(MOONGTANGE_EXPECT_STANDALONE_ASIO_IOCP)
#if !defined(ASIO_HAS_IOCP)
#error "MoongTangE expected Standalone Asio to use IOCP on Windows"
#endif
#elif defined(MOONGTANGE_EXPECT_STANDALONE_ASIO_EPOLL)
#if !defined(ASIO_HAS_EPOLL)
#error "MoongTangE expected Standalone Asio to use epoll on Linux"
#endif
#else
#error "MoongTangE Standalone Asio backend expectation was not configured"
#endif
