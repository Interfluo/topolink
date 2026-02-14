#pragma once

#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <functional>

class BannerWidget : public QWidget {
  Q_OBJECT

public:
  explicit BannerWidget(QWidget *parent = nullptr);

  // Set the current mode in the dropdown without triggering signals (e.g. from
  // shortcut)
  void setMode(int index);
  int currentMode() const;

  // Context Buttons
  void addContextButton(const QString &text, const QString &iconPath,
                        std::function<void()> onClick);
  void clearContextButtons();

signals:
  void modeChanged(int index);
  void importRequested();
  void saveRequested();
  void consoleToggleRequested();
  // potential settingsRequested() etc.

private slots:
  void onModeIndexChanged(int index);

private:
  QLabel *m_titleLabel;
  QComboBox *m_modeCombo;
  QHBoxLayout *m_contextLayout; // Area for dynamic buttons
  QPushButton *m_importBtn;
  QPushButton *m_saveBtn;
  QPushButton *m_consoleBtn;
};
