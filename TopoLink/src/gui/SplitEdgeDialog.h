#ifndef SPLITEDGEDIALOG_H
#define SPLITEDGEDIALOG_H

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

class SplitEdgeDialog : public QDialog {
  Q_OBJECT

public:
  explicit SplitEdgeDialog(QWidget *parent = nullptr) : QDialog(parent) {
    setWindowTitle(tr("Split Edge"));
    setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Normalized position along edge:")));

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, 100);
    m_slider->setValue(50);
    layout->addWidget(m_slider);

    m_valueLabel = new QLabel("0.50");
    m_valueLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_valueLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *okBtn = new QPushButton(tr("OK"));
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(m_slider, &QSlider::valueChanged, this,
            &SplitEdgeDialog::onSliderChanged);
    connect(okBtn, &QPushButton::clicked, this, &SplitEdgeDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &SplitEdgeDialog::reject);
  }

  double getNormalizedValue() const { return m_slider->value() / 100.0; }

signals:
  void valueChanged(double t);

private slots:
  void onSliderChanged(int value) {
    double t = value / 100.0;
    m_valueLabel->setText(QString::number(t, 'f', 2));
    emit valueChanged(t);
  }

private:
  QSlider *m_slider;
  QLabel *m_valueLabel;
};

#endif // SPLITEDGEDIALOG_H
