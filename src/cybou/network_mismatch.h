#ifndef CYBOU_NETWORK_MISMATCH_H
#define CYBOU_NETWORK_MISMATCH_H

#include <stdexcept>

namespace cybou {

/// \brief Локальные сетевые данные принадлежат другой official network (другой NetworkBinding).
/// \details Отдельный тип позволяет desktop'у выполнить cutover: убрать чужие
///          network-bound данные и начать с чистого состояния, не путая это с повреждением.
class NetworkMismatchError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace cybou

#endif // CYBOU_NETWORK_MISMATCH_H
