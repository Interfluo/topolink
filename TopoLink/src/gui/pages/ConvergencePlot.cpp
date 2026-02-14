#include "ConvergencePlot.h"
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <cmath>

ConvergencePlot::ConvergencePlot(QWidget *parent) : QWidget(parent) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setMinimumHeight(200);
}

void ConvergencePlot::addPoint(int id, int iter, double value) {
  if (!m_series.contains(id)) {
    Series s;
    s.color = getColorForId(id);
    s.label = id < 0 ? QString("Edge %1").arg(-id) : QString("Face %1").arg(id);
    m_series.insert(id, s);
  }

  m_series[id].values.append(value);

  if (iter > m_maxIter)
    m_maxIter = iter;
  if (value > m_maxValue)
    m_maxValue = value;
  // We don't update m_minValue here to keep the log scale stable-ish

  update();
}

void ConvergencePlot::clear() {
  m_series.clear();
  m_maxIter = 0;
  m_maxValue = 1.0;
  update();
}

QColor ConvergencePlot::getColorForId(int id) {
  if (id < 0)
    return QColor(0, 120, 215); // Consistent Blue
  return QColor(0, 150, 0);     // Consistent Green
}

void ConvergencePlot::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QRect plotRect = rect().adjusted(45, 10, -10, -35);

  // Draw Legend
  painter.setPen(QColor(0, 120, 215));
  painter.drawText(plotRect.left() + 5, plotRect.bottom() + 20, "Edges (Blue)");
  painter.setPen(QColor(0, 150, 0));
  painter.drawText(plotRect.left() + 100, plotRect.bottom() + 20,
                   "Faces (Green)");

  // Draw background
  painter.fillRect(plotRect, Qt::black);

  if (m_series.isEmpty()) {
    painter.setPen(Qt::gray);
    painter.drawText(plotRect, Qt::AlignCenter, "Waiting for data...");
    return;
  }

  // Draw Grid (Log Scale Y)
  painter.setPen(QColor(60, 60, 60));
  double logMin = std::log10(m_minValue);
  double logMax = std::log10(std::max(m_maxValue, 1.1 * m_minValue));

  for (double power = std::floor(logMin); power <= std::ceil(logMax);
       power += 1.0) {
    double val = std::pow(10, power);
    double yRel = (power - logMin) / (logMax - logMin);
    int y = plotRect.bottom() - yRel * plotRect.height();

    if (y >= plotRect.top() && y <= plotRect.bottom()) {
      painter.drawLine(plotRect.left(), y, plotRect.right(), y);
      painter.drawText(5, y + 5, QString("1e%1").arg(power));
    }
  }

  // Draw Series
  for (auto it = m_series.begin(); it != m_series.end(); ++it) {
    const Series &s = it.value();
    if (s.values.isEmpty())
      continue;

    painter.setPen(QPen(s.color, 1.5));
    QPainterPath path;

    for (int i = 0; i < s.values.size(); ++i) {
      double xRel = m_maxIter > 0 ? (double)i / m_maxIter : 0.0;
      double val = std::max(s.values[i], m_minValue);
      double yRel = (std::log10(val) - logMin) / (logMax - logMin);

      QPoint p(plotRect.left() + xRel * plotRect.width(),
               plotRect.bottom() - yRel * plotRect.height());

      if (i == 0)
        path.moveTo(p);
      else
        path.lineTo(p);
    }
    painter.drawPath(path);
  }

  // Draw axis labels
  painter.setPen(Qt::white);
  painter.drawText(rect().adjusted(0, 0, 0, -5),
                   Qt::AlignBottom | Qt::AlignHCenter, "Iterations");
}
