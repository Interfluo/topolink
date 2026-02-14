#pragma once

#include <QColor>
#include <QMap>
#include <QVector>
#include <QWidget>

class ConvergencePlot : public QWidget {
  Q_OBJECT
public:
  explicit ConvergencePlot(QWidget *parent = nullptr);

  void addPoint(int id, int iter, double value);
  void clear();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct Series {
    QVector<double> values;
    QColor color;
    QString label;
  };

  QMap<int, Series> m_series;
  int m_maxIter = 0;
  double m_maxValue = 1.0;
  double m_minValue = 1e-10;

  QColor getColorForId(int id);
};
