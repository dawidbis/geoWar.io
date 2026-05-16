#pragma once
#include <shared/net/Message.hpp>

#include <memory>
#include <string>
#include <cstdint>

namespace gs::server::net {
    class Connection;
    using ConnectionPtr = std::shared_ptr<Connection>;
}

namespace gs::server::management {

    /// Reprezentuje pojedynczego gracza lub bota w systemie (lobby/gra).
    /// Oddziela to¿samoœæ gracza od jego fizycznego po³¹czenia TCP.
    class Player {
    public:
        Player(uint32_t entityId, std::string name, bool isBot, net::ConnectionPtr conn = nullptr);

        // Gettery
        uint32_t entityId() const noexcept { return entityId_; }
        const std::string& name() const noexcept { return name_; }
        bool isBot() const noexcept { return isBot_; }
        bool isReady() const noexcept { return isReady_; }

        // Settery
        void setReady(bool ready) noexcept { isReady_ = ready; }

        // Zarz¹dzanie po³¹czeniem
        net::ConnectionPtr connection() const { return connection_; }
        void setConnection(net::ConnectionPtr conn);
        bool isConnected() const;

        /// Wyœlij wiadomoœæ do gracza (tylko jeœli jest po³¹czony)
        /// Metoda oznaczona jako const, nie modyfikuje samego gracza.
        void send(const Message& msg) const;

        /// Bezpieczne roz³¹czenie gracza
        void disconnect();

    private:
        uint32_t entityId_;
        std::string name_;
        bool isBot_;
        bool isReady_{ false };

        net::ConnectionPtr connection_;
    };

} // namespace gs::server::management