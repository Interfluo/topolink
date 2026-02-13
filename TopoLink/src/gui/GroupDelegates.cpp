#include "GroupDelegates.h"
#include <QComboBox>
#include <QPainter>

// ============================================================================
// ColorDelegate
// ============================================================================

const QList<QPair<QString, QColor>> ColorDelegate::s_colors = {
    {"Red", QColor(255, 80, 80)},     {"Green", QColor(80, 200, 80)},
    {"Blue", QColor(80, 120, 255)},   {"Yellow", QColor(255, 220, 80)},
    {"Orange", QColor(255, 160, 60)}, {"Purple", QColor(180, 100, 220)},
    {"Cyan", QColor(80, 220, 220)},   {"Magenta", QColor(220, 80, 180)},
    {"Gray", QColor(140, 140, 140)},  {"White", QColor(240, 240, 240)}};

ColorDelegate::ColorDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void ColorDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const {
  QColor color = index.data(Qt::UserRole).value<QColor>();
  painter->save();
  painter->fillRect(option.rect, color);
  painter->setPen(Qt::darkGray);
  painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
  painter->restore();
}

QWidget *ColorDelegate::createEditor(QWidget *parent,
                                     const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  QComboBox *combo = new QComboBox(parent);
  for (const auto &colorPair : s_colors) {
    QPixmap px(16, 16);
    px.fill(colorPair.second);
    combo->addItem(QIcon(px), colorPair.first, colorPair.second);
  }
  return combo;
}

void ColorDelegate::setEditorData(QWidget *editor,
                                  const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  QColor currentColor = index.data(Qt::UserRole).value<QColor>();
  for (int i = 0; i < s_colors.size(); ++i) {
    if (s_colors[i].second == currentColor) {
      combo->setCurrentIndex(i);
      return;
    }
  }
  combo->setCurrentIndex(0);
}

void ColorDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                 const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  QColor color = combo->currentData().value<QColor>();
  model->setData(index, color, Qt::EditRole);
}

// ============================================================================
// RenderModeDelegate
// ============================================================================

RenderModeDelegate::RenderModeDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QWidget *RenderModeDelegate::createEditor(QWidget *parent,
                                          const QStyleOptionViewItem &option,
                                          const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  QComboBox *combo = new QComboBox(parent);
  combo->addItem("Shaded", static_cast<int>(RenderMode::Shaded));
  combo->addItem("Translucent", static_cast<int>(RenderMode::Translucent));
  combo->addItem("Hidden", static_cast<int>(RenderMode::Hidden));
  return combo;
}

void RenderModeDelegate::setEditorData(QWidget *editor,
                                       const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  int mode = index.data(Qt::UserRole).toInt();
  combo->setCurrentIndex(mode);
}

void RenderModeDelegate::setModelData(QWidget *editor,
                                      QAbstractItemModel *model,
                                      const QModelIndex &index) const {
  QComboBox *combo = qobject_cast<QComboBox *>(editor);
  if (!combo)
    return;
  int mode = combo->currentData().toInt();
  model->setData(index, mode, Qt::EditRole);
}
