#include "server/management/Player.hpp"
#include "server/net/Connection.hpp" 

namespace gs::server::management {

    Player::Player(uint32_t entityId, std::string name, bool isBot, net::ConnectionPtr conn)
        : entityId_(entityId)
        , name_(std::move(name))
        , isBot_(isBot)
    {
        // ZMIANA KRYTYCZNA: Bezpieczne atomowe zapisanie wskaünika wspÛ≥dzielonego
        std::atomic_store(&connection_, conn);
    }

    void Player::setConnection(net::ConnectionPtr conn) {
        std::atomic_store(&connection_, conn);
    }

    bool Player::isConnected() const {
        // ZMIANA KRYTYCZNA: Prawdziwe, bezpieczne wπtkowo pobranie kopii shared_ptr.
        // Zapobiega uszkodzeniu bloku kontrolnego, jeúli sieÊ roz≥πczy bota w tym samym u≥amku sekundy.
        auto conn = std::atomic_load(&connection_);
        if (conn) {
            return conn->isAlive();
        }
        return false;
    }

    void Player::send(const Message& msg) const {
        auto conn = std::atomic_load(&connection_);
        if (conn) {
            if (conn->isAlive()) {
                conn->send(msg);
            }
        }
    }

    void Player::disconnect() {
        auto conn = std::atomic_load(&connection_);
        if (conn) {
            conn->disconnect();
        }
        // Atomowe wyczyszczenie wskaünika
        std::atomic_store(&connection_, net::ConnectionPtr(nullptr));
    }

} // namespace gs::server::management