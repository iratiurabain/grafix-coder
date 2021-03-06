#include "FixationsListModel.h"
#include "MyConstants.h"

FixationsListModel::FixationsListModel()
{
    defaultFont = QFont("Times", 9, QFont::Medium);
    highlightFont = QFont("Times", 9, QFont::Bold);
    highlightedRow = -1;

}

int FixationsListModel::rowCount(const QModelIndex &) const {
    return (int)fixations.n_rows;
}
int FixationsListModel::columnCount(const QModelIndex &) const {
    return 4;
}
QVariant FixationsListModel::data(const QModelIndex &index, int role) const {

    int row = index.row();

    if (role == Qt::FontRole) {
        return (row == highlightedRow) ? highlightFont : defaultFont;
    } else if (role != Qt::DisplayRole)
        return QVariant();


    double value;

    switch(index.column()) {
        case 0:
            value = fixations.at(row,FIXCOL_START);
        break;
        case 1:
            value = fixations.at(row,FIXCOL_END);
        break;
        case 2:
            value = fixations.at(row,FIXCOL_DURATION);
        break;
        case 3:
            value = fixations.at(row,FIXCOL_RMS);
        break;
        default:
            value = -1;
    }

    return value;
}
QVariant FixationsListModel::headerData(int section, Qt::Orientation orientation, int role) const {

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return tr("From");
            case 1:
                return tr("To");
            case 2:
                return tr("Duration");
            case 3:
                return tr("RMS");
            default:
                return QVariant();
        }
    }

    if (role == Qt::DisplayRole && orientation == Qt::Vertical) {
        return section;
    }

    return QVariant();
}

void FixationsListModel::updateWithFixations(mat &newFixations, int highlight) {
    this->beginResetModel();
    highlightedRow = highlight;
    fixations = newFixations;
    this->endResetModel();
}
