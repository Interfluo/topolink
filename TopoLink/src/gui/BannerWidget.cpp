#include "BannerWidget.h"
#include <QDebug>

BannerWidget::BannerWidget(QWidget *parent) : QWidget(parent) {
  setFixedHeight(50);
  setStyleSheet("background-color: #2D2D30; color: #F0F0F0; border-bottom: 2px "
                "solid #3E3E42;");

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(15, 5, 15, 5);
  layout->setSpacing(20);

  // 1. Logo / Title
  m_titleLabel = new QLabel("TopoLink", this);
  m_titleLabel->setStyleSheet(
      "font-weight: bold; font-size: 16px; color: #00ACC1;"); // Cyan accent
  layout->addWidget(m_titleLabel);

  // 2. Mode Switcher (Center)
  layout->addStretch();

  QLabel *modeLabel = new QLabel("Mode:", this);
  modeLabel->setStyleSheet("color: #AAAAAA; font-size: 12px;");
  layout->addWidget(modeLabel);

  m_modeCombo = new QComboBox(this);
  m_modeCombo->addItem("1. Geometry Definition");
  m_modeCombo->addItem("2. Topology Definition");
  m_modeCombo->addItem("3. Smoother & Export");

  // Style the combobox
  m_modeCombo->setStyleSheet(
      "QComboBox { "
      "   background-color: #3E3E42; "
      "   border: 1px solid #555; "
      "   border-radius: 4px; "
      "   padding: 5px 10px; "
      "   min-width: 150px; "
      "}"
      "QComboBox::drop-down { border: none; }"
      "QComboBox::down-arrow { image: none; border-left: 1px solid #555; "
      "width: 0px; }" // Minimalist
  );

  layout->addWidget(m_modeCombo);

  // Context Buttons Area
  m_contextLayout = new QHBoxLayout();
  m_contextLayout->setSpacing(10);
  m_contextLayout->setContentsMargins(20, 0, 0, 0); // Spacing from combo
  layout->addLayout(m_contextLayout);

  layout->addStretch();

  // 3. Right Actions
  m_consoleBtn = new QPushButton("Console", this);
  m_importBtn = new QPushButton("Import", this);
  m_saveBtn = new QPushButton("Save", this);

  QString btnStyle = "QPushButton { "
                     "   background-color: transparent; "
                     "   border: 1px solid #555; "
                     "   border-radius: 4px; "
                     "   padding: 5px 15px; "
                     "}"
                     "QPushButton:hover { background-color: #3E3E42; }"
                     "QPushButton:pressed { background-color: #505050; }";

  m_consoleBtn->setStyleSheet(
      "QPushButton { "
      "   background-color: transparent; "
      "   border: 1px solid #00ACC1; "
      "   color: #00ACC1; "
      "   border-radius: 4px; "
      "   padding: 5px 15px; "
      "}"
      "QPushButton:hover { background-color: #00ACC1; color: white; }"
      "QPushButton:pressed { background-color: #00838F; }");

  m_importBtn->setStyleSheet(btnStyle);
  m_saveBtn->setStyleSheet(btnStyle);

  layout->addWidget(m_consoleBtn);
  layout->addWidget(m_importBtn);
  layout->addWidget(m_saveBtn);

  // Connect signals
  connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &BannerWidget::onModeIndexChanged);

  connect(m_consoleBtn, &QPushButton::clicked, this,
          &BannerWidget::consoleToggleRequested);
  connect(m_importBtn, &QPushButton::clicked, this,
          &BannerWidget::importRequested);
  connect(m_saveBtn, &QPushButton::clicked, this, &BannerWidget::saveRequested);
}

void BannerWidget::setMode(int index) {
  if (index >= 0 && index < m_modeCombo->count()) {
    QSignalBlocker blocker(
        m_modeCombo); // Prevent re-triggering loop if calling from outside
    m_modeCombo->setCurrentIndex(index);
  }
}

int BannerWidget::currentMode() const { return m_modeCombo->currentIndex(); }

void BannerWidget::onModeIndexChanged(int index) { emit modeChanged(index); }

void BannerWidget::addContextButton(const QString &text,
                                    const QString &iconPath,
                                    std::function<void()> onClick) {
  QPushButton *btn = new QPushButton(text, this);

  if (!iconPath.isEmpty()) {
    QIcon icon(iconPath);
    if (!icon.isNull()) {
      btn->setIcon(icon);
      btn->setIconSize(QSize(24, 24));
    }
  }

  // Style specifically for context buttons (maybe slightly different accent?)
  btn->setStyleSheet(
      "QPushButton { "
      "   background-color: #3E3E42; "
      "   color: #00ACC1; " // Cyan text
      "   border: 1px solid #00ACC1; "
      "   border-radius: 4px; "
      "   padding: 5px 15px; "
      "   font-weight: bold; "
      "   text-align: left; "
      "}"
      "QPushButton:hover { background-color: #00ACC1; color: white; }"
      "QPushButton:pressed { background-color: #00838F; }");

  connect(btn, &QPushButton::clicked, this, onClick);
  m_contextLayout->addWidget(btn);
}

void BannerWidget::clearContextButtons() {
  QLayoutItem *item;
  while ((item = m_contextLayout->takeAt(0)) != nullptr) {
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
}
