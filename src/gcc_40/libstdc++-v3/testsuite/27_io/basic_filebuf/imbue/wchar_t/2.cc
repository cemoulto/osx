// 2003-05-13 Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// 27.8.1.4 Overridden virtual functions

#include <fstream>
#include <locale>
#include <testsuite_hooks.h>

void test02()
{
  using namespace std;
  bool test __attribute__((unused)) = true;
  const char name_01[] = "filebuf_virtuals-1.txt"; // file with data in it

  locale loc;
  wfilebuf ob;
  VERIFY( ob.getloc() == loc );
  ob.open(name_01, ios_base::in);
  VERIFY( ob.is_open() );
 
  typedef streambuf::pos_type pos_type;
  pos_type bad = pos_type(streambuf::off_type(-1));
  pos_type p = ob.pubseekoff(2, ios_base::beg, ios_base::in);
  VERIFY( p != bad);

  // According to 27.5.2.2.1, p1, loc == getloc() after pubimbue(loc).
  locale loc_de = __gnu_test::try_named_locale("de_DE");
  locale ret = ob.pubimbue(loc_de);
  VERIFY( ob.getloc() == loc_de );
}

int main() 
{
  test02();
  return 0;
}
