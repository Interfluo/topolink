#include "SmootherPage.h"
#include <QPushButton>

SmootherPage::SmootherPage(QWidget *parent) : QWidget(parent) { setupUI(); }

void SmootherPage::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QLabel *header = new QLabel("Solver Configuration");
  header->setStyleSheet(
      "font-weight: bold; font-size: 16px; margin-bottom: 10px;");
  mainLayout->addWidget(header);

  QGroupBox *edgeGroup = new QGroupBox("Edge Solver");
  QFormLayout *edgeLayout = new QFormLayout(edgeGroup);
  m_edgeIters = new QSpinBox();
  m_edgeIters->setRange(1, 10000);
  m_edgeIters->setValue(1000);
  m_edgeRelax = new QDoubleSpinBox();
  m_edgeRelax->setRange(0.0, 1.0);
  m_edgeRelax->setSingleStep(0.05);
  m_edgeRelax->setValue(0.9);
  m_edgeBCRelax = new QDoubleSpinBox();
  m_edgeBCRelax->setRange(0.0, 1.0);
  m_edgeBCRelax->setSingleStep(0.05);
  m_edgeBCRelax->setValue(0.1);
  edgeLayout->addRow("Iterations:", m_edgeIters);
  edgeLayout->addRow("Relaxation:", m_edgeRelax);
  edgeLayout->addRow("BC Relaxation:", m_edgeBCRelax);
  mainLayout->addWidget(edgeGroup);

  QGroupBox *faceGroup = new QGroupBox("Face Solver");
  QFormLayout *faceLayout = new QFormLayout(faceGroup);
  m_faceIters = new QSpinBox();
  m_faceIters->setRange(1, 10000);
  m_faceIters->setValue(1000);
  m_faceRelax = new QDoubleSpinBox();
  m_faceRelax->setRange(0.0, 1.0);
  m_faceRelax->setSingleStep(0.05);
  m_faceRelax->setValue(0.9);
  m_faceBCRelax = new QDoubleSpinBox();
  m_faceBCRelax->setRange(0.0, 1.0);
  m_faceBCRelax->setSingleStep(0.05);
  m_faceBCRelax->setValue(0.1);
  faceLayout->addRow("Iterations:", m_faceIters);
  faceLayout->addRow("Relaxation:", m_faceRelax);
  faceLayout->addRow("BC Relaxation:", m_faceBCRelax);
  mainLayout->addWidget(faceGroup);

  QGroupBox *miscGroup = new QGroupBox("Global parameters");
  QFormLayout *miscLayout = new QFormLayout(miscGroup);
  m_singularityRelax = new QDoubleSpinBox();
  m_singularityRelax->setRange(0.0, 1.0);
  m_singularityRelax->setValue(0.1);
  m_growthRateRelax = new QDoubleSpinBox();
  m_growthRateRelax->setRange(0.0, 0.1);
  m_growthRateRelax->setDecimals(4);
  m_growthRateRelax->setValue(0.002);
  m_subIters = new QSpinBox();
  m_subIters->setRange(1, 16);
  m_subIters->setValue(4);
  m_projFreq = new QSpinBox();
  m_projFreq->setRange(1, 100);
  m_projFreq->setValue(1);
  miscLayout->addRow("Singularity Relax:", m_singularityRelax);
  miscLayout->addRow("Growth Rate Relax:", m_growthRateRelax);
  miscLayout->addRow("Sub-iterations:", m_subIters);
  miscLayout->addRow("Proj. Frequency:", m_projFreq);
  mainLayout->addWidget(miscGroup);

  mainLayout->addStretch();

  QPushButton *runBtn = new QPushButton("Run Solver");
  runBtn->setStyleSheet("background-color: #0078d7; color: white; font-weight: "
                        "bold; padding: 10px;");
  mainLayout->addWidget(runBtn);
}
