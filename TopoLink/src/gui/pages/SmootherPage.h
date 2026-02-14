#pragma once
#include "../../core/SmootherConfig.h"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

class ConvergencePlot;

class SmootherPage : public QWidget {
  Q_OBJECT
public:
  explicit SmootherPage(QWidget *parent = nullptr);

  SmootherConfig getConfig() const;

  ConvergencePlot *plot() const { return m_plot; }
  QPushButton *runButton() const { return m_runBtn; }
  void setStatusText(const QString &text);

signals:
  void runSolverRequested();

private:
  void setupUI();

  ConvergencePlot *m_plot;
  QPushButton *m_runBtn;
  QLabel *m_statusLabel;

  QSpinBox *m_edgeIters;
  QDoubleSpinBox *m_edgeRelax;
  QDoubleSpinBox *m_edgeBCRelax;
  QSpinBox *m_faceIters;
  QDoubleSpinBox *m_faceRelax;
  QDoubleSpinBox *m_faceBCRelax;
  QDoubleSpinBox *m_singularityRelax;
  QDoubleSpinBox *m_growthRateRelax;
  QSpinBox *m_subIters;
  QSpinBox *m_projFreq;
};
