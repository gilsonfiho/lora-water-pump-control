#pragma once
// ============================================================================
//  Observer.h  -  Sinal/observador leve para eventos (padrao Observer).
// ----------------------------------------------------------------------------
//  Usado, por exemplo, para notificar quando o estado do sensor de nivel muda,
//  desacoplando quem detecta o evento de quem reage a ele. Numero de inscritos
//  fixo para evitar alocacao dinamica no ESP32-C3.
// ============================================================================

#include <cstdint>
#include <functional>

namespace core {

template <typename Event, uint8_t kMaxObservers = 4>
class Signal {
public:
    using Handler = std::function<void(const Event&)>;

    // Inscreve um observador. Retorna false se a lista estiver cheia.
    bool subscribe(Handler handler) {
        if (count_ >= kMaxObservers) return false;
        handlers_[count_++] = std::move(handler);
        return true;
    }

    // Notifica todos os inscritos na ordem de inscricao.
    void emit(const Event& event) const {
        for (uint8_t i = 0; i < count_; ++i) handlers_[i](event);
    }

private:
    Handler handlers_[kMaxObservers];
    uint8_t count_ = 0;
};

}  // namespace core
