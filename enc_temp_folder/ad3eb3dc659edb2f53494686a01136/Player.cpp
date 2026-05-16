#include "server/management/Player.hpp"
#include "server/net/Connection.hpp" 

namespace gs::server::management {

    Player::Player(uint32_t entityId, std::string name, bool isBot, net::ConnectionPtr conn)
        : entityId_(entityId)
        , name_(std::move(name))
        , isBot_(isBot)
        , connection_(std::move(conn))
    {
    }

    void Player::setConnection(net::ConnectionPtr conn) {
        connection_ = std::move(conn);
    }

    bool Player::isConnected() const {
        // ZABEZPIECZENIE: Pobieramy lokaln¹ kopiê shared_ptr.
        // Jeœli inny w¹tek zresetuje connection_ w tym samym momencie,
        // nasza lokalna kopia (conn) nadal bêdzie trzymaæ obiekt przy ¿yciu na czas testu.
        if (auto conn = connection_) {
            return conn->isAlive();
        }
        return false;
    }

    void Player::send(const Message& msg) const {
        // ZABEZPIECZENIE (Brak TOCTOU): 
        // Przed prób¹ dostêpu lokalnie kopiujemy referencjê.
        if (auto conn = connection_) {
            // Skoro weszliœmy do tego bloku, mamy GWARANCJÊ, ¿e 'conn' nie jest nullptr.
            if (conn->isAlive()) {
                conn->send(msg);
            }
        }
    }

    void Player::disconnect() {
        if (auto conn = connection_) {
            conn->disconnect();
        }

        // Zwalniamy wskaŸnik, dziêki czemu po³¹czenie mo¿e zostaæ bezpowrotnie
        // usuniête z pamiêci, gdy tylko skoñcz¹ siê jego asynchroniczne operacje I/O.
        connection_.reset();
    }

} // namespace gs::server::management