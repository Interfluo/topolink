#pragma once

#include <QColor>
#include <QList>
#include <QPair>
#include <QStyledItemDelegate>

// Shared Enum for Render Modes
enum class RenderMode { Shaded = 0, Translucent = 1, Hidden = 2 };

// ============================================================================
// ColorDelegate
// ============================================================================
class ColorDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit ColorDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;
  void setEditorData(QWidget *editor, const QModelIndex &index) const override;
  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override;

private:
  static const QList<QPair<QString, QColor>> s_colors;
};

// ============================================================================
// RenderModeDelegate
// ============================================================================
class RenderModeDelegate : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit RenderModeDelegate(QObject *parent = nullptr);

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;
  void setEditorData(QWidget *editor, const QModelIndex &index) const override;
  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const override;
};
