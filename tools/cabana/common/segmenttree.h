#pragma once

#include <QPointF>
#include <QVector>
#include <utility>
#include <vector>

class SegmentTree {
public:
  SegmentTree() = default;
  void build(const QVector<QPointF> &arr);
  inline std::pair<double, double> minmax(int left, int right) const { return get_minmax(1, 0, size - 1, left, right); }

private:
  std::pair<double, double> get_minmax(int n, int left, int right, int range_left, int range_right) const;
  void build_tree(const QVector<QPointF> &arr, int n, int left, int right);
  std::vector<std::pair<double, double>> tree;
  int size = 0;
};
