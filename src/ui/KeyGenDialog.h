#pragma once
#include <QDialog>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

namespace macxterm::ui {

// SSH key generator (MobaXterm's MobaKeyGen). Wraps the system `ssh-keygen` to
// create a key pair of the chosen type; shows the resulting public key.
class KeyGenDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeyGenDialog(QWidget* parent = nullptr);

private slots:
    void generate();
    void browse();

private:
    QComboBox* m_type = nullptr;
    QLineEdit* m_path = nullptr;
    QLineEdit* m_passphrase = nullptr;
    QPlainTextEdit* m_output = nullptr;
};

} // namespace macxterm::ui
