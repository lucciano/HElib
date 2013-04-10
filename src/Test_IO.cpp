/* Copyright (C) 2012,2013 IBM Corp.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "NTL/zz_pX.h"
#include "FHE.h"
#include "timing.h"
#include "EncryptedArray.h"
#include <fstream>

#define N_TESTS 5
static long ms[N_TESTS][4] = {
  //nSlots  m   phi(m) ord(2)
  {   2,    7,    6,    3},
  {   6,   31,   30,    5},
  { 256, 4369, 4096,   16}, // gens=129(16),3(!16)
  {  378,  5461,  5292, 14}, // gens=3(126),509(3)
  //  {  630,  8191,  8190, 13}, // gens=39(630)
  //  {  600, 13981, 12000, 20}, // gens=10(30),23(10),3(!2)
  {  682, 15709, 15004, 22}}; // gens=5(682)

void checkCiphertext(const Ctxt& ctxt, const ZZX& ptxt, const FHESecKey& sk);

int main(int argc, char *argv[]) 
{
  long c = 1;
  long w = 5;
  long ptxtSpace=2;

  FHEcontext* contexts[N_TESTS];
  FHESecKey*  sKeys[N_TESTS];
  Ctxt*       ctxts[N_TESTS];
  EncryptedArray* eas[N_TESTS];
  vector<GF2X> ptxts[N_TESTS];


  // first loop: generate stuff and write it to cout

  // open file for writing
  {fstream keyFile("iotest.txt", fstream::out|fstream::trunc);
   assert(keyFile.is_open());
  for (long i=0; i<N_TESTS; i++) {
    long m = ms[i][1];

    contexts[i] = new FHEcontext(m);
    buildModChain(*contexts[i], 2, c);  // Set the modulus chain

    keyFile << *contexts[i] << endl;

    sKeys[i] = new FHESecKey(*contexts[i]);
    const FHEPubKey& publicKey = *sKeys[i];
    sKeys[i]->GenSecKey(w,ptxtSpace); // A Hamming-weight-w secret key

    cerr << "generating key-switching matrices... ";
    addSome1DMatrices(*sKeys[i]);// compute key-switching matrices that we need
    cerr << "done\n";

    cerr << "computing masks and tables for rotation...";
    eas[i] = new EncryptedArray(*contexts[i]);
    cerr << "done\n";
    long nslots = eas[i]->size();

    keyFile << *sKeys[i] << endl;;
    keyFile << *sKeys[i] << endl;;
    keyFile << *eas[i] << endl;

    vector<GF2X> b;
    eas[i]->random(ptxts[i]);
    ctxts[i] = new Ctxt(publicKey);
    eas[i]->encrypt(*ctxts[i], publicKey, ptxts[i]);
    eas[i]->decrypt(*ctxts[i], *sKeys[i], b);
    assert(ptxts[i].size() == b.size());
    for (long j = 0; j < nslots; j++) assert (ptxts[i][j] == b[j]);

    // output a
    keyFile << "[ ";
    for (long j = 0; j < nslots; j++) keyFile << ptxts[i][j] << " ";
    keyFile << "]\n";

    ZZX poly;
    eas[i]->encode(poly,ptxts[i]);
    keyFile << poly << endl;
    
    keyFile << *ctxts[i] << endl;
    keyFile << *ctxts[i] << endl;
    cerr << "okay " << i << endl;
  }
  keyFile.close();}
  cerr << "so far, so good\n";

  // second loop: read from input and repeat the computation

  // open file for read
  {fstream keyFile("iotest.txt", fstream::in);
  for (long i=0; i<(long)(sizeof(ms)/sizeof(int[4])); i++) {

    FHEcontext& context = *contexts[i];
    FHEcontext tmpContext(ms[i][1]);
    keyFile >> tmpContext;
    assert (context == tmpContext);
    cerr << i << ": context matches input\n";

    FHESecKey secretKey(context);
    FHESecKey secretKey2(tmpContext);
    const FHEPubKey& publicKey = secretKey;
    const FHEPubKey& publicKey2 = secretKey2;

    keyFile >> secretKey;
    keyFile >> secretKey2;
    assert(secretKey == *sKeys[i]);
    cerr << "   secret key matches input\n";

    EncryptedArray ea(context);
    EncryptedArray ea2(tmpContext);
    keyFile >> ea;
    assert(ea==*eas[i]);
    cerr << "   ea matches input\n";

    long nslots = ea.size();

    vector<GF2X> a;
    a.resize(nslots);
    assert(nslots == (long)ptxts[i].size());
    seekPastChar(keyFile, '['); // defined in NumbTh.cpp
    for (long j = 0; j < nslots; j++) { 
      keyFile >> a[j];
      assert(a[j] == ptxts[i][j]);
    }
    seekPastChar(keyFile, ']');
    cerr << "   ptxt matches input\n";

    ZZX poly1, poly2;
    keyFile >> poly1;
    eas[i]->encode(poly2,a);
    assert(poly1 == poly2);
    cerr << "   eas[i].encode(a)==poly1 okay\n";

    ea.encode(poly2,a);
    assert(poly1 == poly2);
    cerr << "   ea.encode(a)==poly1 okay\n";

    ea2.encode(poly2,a);
    assert(poly1 == poly2);
    cerr << "   ea2.encode(a)==poly1 okay\n";

    eas[i]->decode(a,poly1);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   eas[i].decode(poly1)==ptxts[i] okay\n";

    ea.decode(a,poly1);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   ea.decode(poly1)==ptxts[i] okay\n";

    ea2.decode(a,poly1);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   ea2.decode(poly1)==ptxts[i] okay\n";

    Ctxt ctxt(publicKey);
    Ctxt ctxt2(publicKey2);
    keyFile >> ctxt;
    keyFile >> ctxt2;
    assert(ctxts[i]->equalsTo(ctxt,/*comparePkeys=*/false));
    cerr << "   ctxt matches input\n";

    sKeys[i]->Decrypt(poly2,*ctxts[i]);
    assert(poly1 == poly2);
    cerr << "   sKeys[i]->decrypt(*ctxts[i]) == poly1 okay\n";

    secretKey.Decrypt(poly2,*ctxts[i]);
    assert(poly1 == poly2);
    cerr << "   secretKey.decrypt(*ctxts[i]) == poly1 okay\n";

    secretKey.Decrypt(poly2,ctxt);
    assert(poly1 == poly2);
    cerr << "   secretKey.decrypt(ctxt) == poly1 okay\n";

    secretKey2.Decrypt(poly2,ctxt2);
    assert(poly1 == poly2);
    cerr << "   secretKey2.decrypt(ctxt2) == poly1 okay\n";

    eas[i]->decrypt(ctxt, *sKeys[i], a);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   eas[i].decrypt(ctxt, *sKeys[i])==ptxts[i] okay\n";

    ea.decrypt(ctxt, secretKey, a);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   ea.decrypt(ctxt, secretKey)==ptxts[i] okay\n";

    ea2.decrypt(ctxt2, secretKey2, a);
    assert(nslots == (long)a.size());
    for (long j = 0; j < nslots; j++) assert(a[j] == ptxts[i][j]);
    cerr << "   ea2.decrypt(ctxt2, secretKey2)==ptxts[i] okay\n";

    cerr << "test "<<i<<" okay\n\n";
  }}
}