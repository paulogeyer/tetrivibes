// Field strings must be exactly 264 cells and use only legal cell characters.
#include "game/Field.h"

#include <QString>
#include <cassert>

int main()
{
    const QString emptyField(tnet::kFieldWidth * tnet::kFieldHeight, QLatin1Char('0'));
    assert(tnet::Field::isValidEncoding(emptyField));
    assert(!tnet::Field::isValidEncoding(emptyField.left(emptyField.size() - 1)));
    assert(!tnet::Field::isValidEncoding(emptyField + QLatin1Char('0')));
    QString invalid = emptyField;
    invalid[0] = QLatin1Char('!');
    assert(!tnet::Field::isValidEncoding(invalid));
    return 0;
}
