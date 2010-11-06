/*
  This file is part of the Grantlee template system.

  Copyright (c) 2009,2010 Stephen Kelly <steveire@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either version
  2.1 of the Licence, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef GRANTLEE_LEXER_P_H
#define GRANTLEE_LEXER_P_H

#include "token.h"

// #include <QtCore/QList>

template <typename T> class QList;

namespace Grantlee
{

class Lexer
{
public:
  Lexer( const QString &templateString );
  ~Lexer();

  QList<Token> tokenize() const;

private:
  enum State {
    InTag,
    NotInTag
  };

  Token createToken( const QString &fragment, State inTag, int *linecount ) const;

  QString m_templateString;

};

}

#endif
