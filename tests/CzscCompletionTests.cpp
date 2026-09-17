// SPDX-License-Identifier: GPL-3.0-or-later
#include "../CzscCore.h"
#include "SseIndexDaily.h"
#include <limits>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace
{
TrendAnchorContract Anchor(int nCount, int nVersion = 2)
{
  TrendAnchorContract C;
  C.nVersion = nVersion;
  for (int i = 0; i < nCount; i++) C.Dates.push_back(nVersion == 1 ? 20200101+i : 20200102);
  return C;
}

const float H[] = {12,13,13,15,14,15,14,10,11,12,11,15,14,15,14,10,11};
const float L[] = {10,11,10,14,13,13,13,9,10,10,10,14,13,13,13,9,10};

bool TestAnchorContracts()
{
  SubTrendHierarchy A = BuildSubTrendHierarchy(5,H,L,Anchor(5,1));
  SubTrendHierarchy B = BuildSubTrendHierarchy(5,H,L,Anchor(5,2));
  if (!A.bAvailable || !B.bAvailable || A.nAnchorVersion == B.nAnchorVersion ||
      A.Nodes.size() != 1 || B.Nodes.size() != 1) return false;
  TrendAnchorContract C = Anchor(5);
  C.Dates[0] = 20000103;
  if (BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  C = Anchor(5); C.Dates[4] = 20221201;
  if (BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  C = Anchor(5); C.Dates[0] = 20000104; C.Dates[4] = 20221130;
  if (!BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  C.Dates[1] = 20200230;
  if (BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  C = Anchor(5,1); C.Dates[1] = C.Dates[0];
  if (BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  C = Anchor(4);
  if (BuildSubTrendHierarchy(5,H,L,C).bAvailable) return false;
  float Bad[] = {12,13,13,15,std::numeric_limits<float>::quiet_NaN()};
  return !BuildSubTrendHierarchy(5,Bad,L,Anchor(5)).bAvailable;
}

bool TestCompletionAndSuccessorSeparated()
{
  const std::vector<SegmentPoint> P;
  const std::vector<Center> C;
  const std::vector<TrendStructure> T;
  SubTrendHierarchy A = BuildSubTrendHierarchy(4,H,L,Anchor(4));
  if (!BuildTrendCompletionEvidence(P,C,T,A,4).empty()) return false;
  A = BuildSubTrendHierarchy(5,H,L,Anchor(5));
  std::vector<TrendCompletionEvidence> E = BuildTrendCompletionEvidence(P,C,T,A,5);
  if (E.size() != 1 || E[0].nConnectionBar != 3 || E[0].nLatestBar != 4 ||
      E[0].nSuccessor != -1 || E[0].nDecompositionRule != 1 || E[0].nReason != 1 ||
      E[0].nTheoreticalLevel != 0 || E[0].nTrendSpace != 1) return false;
  A = BuildSubTrendHierarchy(7,H,L,Anchor(7));
  E = BuildTrendCompletionEvidence(P,C,T,A,7);
  if (E.size() != 1 || E[0].nLatestBar != 4 || E[0].nSuccessorEstablishedAt != 6 ||
      E[0].nObservedAt != 6 || A.Nodes[1].nStart != 3) return false;
  std::vector<ZhongYinEvidence> Z = BuildZhongYinEvidence(E,7,0);
  return Z.size() == 2 && Z[0].nEnter == 4 && Z[0].nEnd == 6 &&
         Z[0].nVersion == 1 && Z[0].bAvailable && Z[1].nVersion == 2 && !Z[1].bAvailable;
}

bool TestTouchReturnGapAndNoCenter()
{
  // 闭区间接触即返回；一次离开不足，返回后须重新取得离开+不返回证据。
  float Hi[] = {12,13,13,15,14,16,15};
  float Lo[] = {10,11,10,14,12,15,14};
  SubTrendHierarchy A = BuildSubTrendHierarchy(5,Hi,Lo,Anchor(5));
  if (A.Nodes.size() != 1 || A.Nodes[0].nCompletedAt != -1) return false;
  A = BuildSubTrendHierarchy(7,Hi,Lo,Anchor(7));
  if (A.Nodes[0].nCompletedAt != 6 || A.Nodes[0].nConnection != 5) return false;
  float GapH[] = {2,4,6,8,10}, GapL[] = {1,3,5,7,9};
  A = BuildSubTrendHierarchy(5,GapH,GapL,Anchor(5));
  if (!A.bAvailable || !A.Nodes.empty()) return false;
  float CrossH[] = {12,13,13,15,9}, CrossL[] = {10,11,10,14,8};
  A=BuildSubTrendHierarchy(5,CrossH,CrossL,Anchor(5));
  if (A.Nodes[0].nCompletedAt!=-1) return false; // 离开后跨到另一侧，不是不返回证据
  // 仅两根重叠不能成枢；价格间隙不伪造第三个单位。
  return BuildSubTrendHierarchy(2,H,L,Anchor(2)).Nodes.empty();
}

bool TestStrictChildrenAndSnapshotIdDrift()
{
  SubTrendHierarchy A = BuildSubTrendHierarchy(13,H,L,Anchor(13));
  int nParent = -1;
  for (std::size_t i = 0; i < A.Nodes.size(); i++)
    if (A.Nodes[i].nLevel == 1) nParent = static_cast<int>(i);
  if (nParent < 0 || A.Nodes[nParent].Children.size() != 3 ||
      A.Nodes[nParent].nEstablishedAt != 12) return false;
  for (std::size_t i = 0; i < A.Nodes[nParent].Children.size(); i++)
  {
    int nChild = A.Nodes[nParent].Children[i];
    if (nChild >= nParent || A.Nodes[nChild].nLevel != 0 || A.Nodes[nChild].nCompletedAt < 0)
      return false;
  }
  SubTrendHierarchy B = BuildSubTrendHierarchy(17,H,L,Anchor(17));
  int nNewParent = -1;
  for (std::size_t i = 0; i < B.Nodes.size(); i++)
    if (B.Nodes[i].nLevel == 1 && B.Nodes[i].nStart == A.Nodes[nParent].nStart)
      nNewParent = static_cast<int>(i);
  // 插入更多锚层节点后数组ID漂移；不是跨前缀稳定身份。
  if (nNewParent <= nParent) return false;
  float RedrawnH[13], RedrawnL[13];
  for (int i = 0; i < 13; i++) { RedrawnH[i]=H[i]; RedrawnL[i]=L[i]; }
  RedrawnL[3]=11; // 端点重绘后不能沿用旧ID的完成证据。
  SubTrendHierarchy R = BuildSubTrendHierarchy(13,RedrawnH,RedrawnL,Anchor(13));
  return R.Nodes[0].nConnection != A.Nodes[0].nConnection;
}

bool TestLegacyGapsOverlapNoCandidates()
{
  std::vector<SegmentPoint> P(3);
  P[0] = SegmentPoint{1,20,12,11,0,0,0};
  P[1] = SegmentPoint{1,25,15,14,0,0,0};
  P[2] = SegmentPoint{-1,28,14,13,0,0,0};
  std::vector<Center> C(2);
  C[0] = Center{0,20,12,10,13,9,1};
  C[1] = Center{30,35,16,13,17,12,1};
  std::vector<TrendStructure> T(2);
  T[0] = TrendStructure{0,0,20,0,0};
  T[1] = TrendStructure{0,30,35,1,1};
  std::vector<TrendCompletionEvidence> E = BuildTrendCompletionEvidence(P,C,T,SubTrendHierarchy(),36);
  if (E.size() != 1 || E[0].nConnectionBar != 25 || E[0].nLatestBar != 28 ||
      E[0].nSuccessorEstablishedAt != 35 || E[0].nTheoreticalLevel != -1 ||
      E[0].nConnectionPoint != 1 || E[0].nDecompositionRule != 1) return false;
  // [20,30] 中选首个合法离开端点25，未使用买卖候选。
  T[1].nStart = 19; // 重叠成员包络不可自动充当串接走势
  if (!BuildTrendCompletionEvidence(P,C,T,SubTrendHierarchy(),36).empty()) return false;
  T.resize(1);
  E = BuildTrendCompletionEvidence(P,C,T,SubTrendHierarchy(),36);
  if (E.size() != 1 || E[0].nSuccessor != -1 || E[0].nObservedAt != 35) return false;
  P.resize(2); // 延迟确认，只有离开端点
  return BuildTrendCompletionEvidence(P,C,T,SubTrendHierarchy(),36).empty();
}

bool TestProjectionNoBackfillAndInvalidContract()
{
  float Hi[7], Lo[7], Out[7];
  for (int i=0;i<7;i++) { Hi[i]=H[i]; Lo[i]=L[i]; }
  float Mode[11] = {970,0,8,2,200102,200102,200102,200102,200102,200102,200102};
  Func30(7,Out,Hi,Lo,Mode);
  for (int i=0;i<6;i++) if (Out[i] != 0) return false;
  if (Out[6] != 7) return false; // observedAt+1，不能写到连接点3或证据点4
  Mode[2]=4;
  Func30(7,Out,Hi,Lo,Mode);
  if (Out[6] != 4) return false;
  Mode[2]=7;
  Func30(7,Out,Hi,Lo,Mode);
  if (Out[6] != 5) return false;
  Mode[1]=0.5f;
  Func30(7,Out,Hi,Lo,Mode);
  for (int i=0;i<7;i++) if (Out[i] != -1) return false;
  Mode[1]=0; Mode[10]=221201;
  Func30(7,Out,Hi,Lo,Mode);
  for (int i=0;i<7;i++) if (Out[i] != -1) return false;
  Mode[10]=200102; Mode[0]=950; Mode[2]=17;
  Func30(7,Out,Hi,Lo,Mode);
  for (int i=0;i<7;i++) if (Out[i] != -1) return false;
  return true;
}

bool TestAllEvidenceProjectionFields()
{
  float Hi[7],Lo[7],Out[7];
  for (int i=0;i<7;i++) { Hi[i]=H[i]; Lo[i]=L[i]; }
  CzscAnalyzer A;
  TrendAnchorContract C=Anchor(7);
  BuildAnalyzerFromPrice(A,7,Hi,Lo,DefaultConfig(),&C);
  const float Node[]={1,0,1,4,1,3,3,4,5,2,0,2,1,15,10,12,11};
  const float Evidence[]={1,1,1,0,4,1,0,5,7,2,7,0,2,1,1};
  const float Main[]={1,1,1,5,7,7,0,1};
  const float Boll[]={2,1,2,5,0,7,0,-1};
  for (int f=0;f<17;f++)
  {
    ApplyTrendEvidenceProjection(7,Out,A,95,0,f);
    if (Out[6]!=Node[f]) return false;
  }
  for (int f=0;f<15;f++)
  {
    ApplyTrendEvidenceProjection(7,Out,A,97,0,f);
    if (Out[6]!=Evidence[f]) return false;
  }
  for (int f=0;f<8;f++)
  {
    ApplyTrendEvidenceProjection(7,Out,A,99,0,f);
    if (Out[6]!=Main[f]) return false;
    ApplyTrendEvidenceProjection(7,Out,A,99,1,f);
    if (Out[6]!=Boll[f]) return false;
  }
  const int Outputs[]={93,94,96,98};
  const float Counts[]={1,2,1,2};
  for (int i=0;i<4;i++)
  {
    ApplyTrendEvidenceProjection(7,Out,A,Outputs[i],0,0);
    if (Out[6]!=Counts[i]) return false;
    for (int j=0;j<6;j++) if (Out[j]!=0) return false;
  }
  ApplyTrendEvidenceProjection(7,Out,A,97,100,0);
  if (Out[6]!=0) return false;
  ApplyTrendEvidenceProjection(7,Out,A,95,0,100);
  if (Out[6]!=0) return false; // 锚层没有伪造子走势
  A.Hierarchy=BuildSubTrendHierarchy(13,H,L,Anchor(13));
  A.nCount=13;
  float ParentOut[13];
  ApplyTrendEvidenceProjection(13,ParentOut,A,95,3,100);
  if (ParentOut[12]!=1) return false;
  ApplyTrendEvidenceProjection(13,ParentOut,A,95,3,102);
  if (ParentOut[12]!=3) return false;
  ApplyTrendEvidenceProjection(13,ParentOut,A,95,3,103);
  return ParentOut[12]==0;
}

bool TestBollIndependentAuxiliary()
{
  TrendCompletionEvidence E = {};
  E.nTrendSpace=1; E.nLatestBar=20; E.nSuccessorEstablishedAt=23; E.nObservedAt=26;
  std::vector<TrendCompletionEvidence> Evidence(1,E);
  std::vector<float> Close(27,10);
  Close[0]=30; Close[1]=30; Close[2]=30; // 20/21/22连续收口
  Close[24]=30; // 24放大，且新中枢已成立
  std::vector<ZhongYinEvidence> Z = BuildZhongYinEvidence(Evidence,27,&Close);
  if (Z.size()!=2 || Z[0].nEnd!=23 || Z[1].nEnd!=24 || Z[1].nContraction!=20 ||
      !Z[1].bAvailable || Z[0].nEnter!=20 || Z[1].nEnter!=20) return false;
  Evidence[0].nSuccessorEstablishedAt=-1;
  Z=BuildZhongYinEvidence(Evidence,27,&Close);
  if (Z[0].nEnd!=-1 || Z[1].nEnd!=-1) return false; // BOLL不能替代新中枢
  Z=BuildZhongYinEvidence(Evidence,27,0);
  return Z[0].bAvailable && !Z[1].bAvailable;
}

bool TestSseMemberAndCandidateIsolation()
{
  std::vector<float> High(SSE_DAILY_HIGH,SSE_DAILY_HIGH+SSE_DAILY_COUNT);
  std::vector<float> Low(SSE_DAILY_LOW,SSE_DAILY_LOW+SSE_DAILY_COUNT);
  TrendAnchorContract C;
  C.nVersion=1;
  for (int i=0;i<SSE_DAILY_COUNT;i++)
  {
    int y=0,m=0,d=0;
    if (std::sscanf(SSE_DAILY_DATE[i],"%d-%d-%d",&y,&m,&d)!=3) return false;
    C.Dates.push_back(y*10000+m*100+d);
  }
  const int Configs[]={0,1100};
  for (int c=0;c<2;c++)
  {
    CzscAnalyzer A,B;
    CzscConfig Config=DecodeConfig(static_cast<float>(Configs[c]));
    BuildAnalyzerFromPrice(A,SSE_DAILY_COUNT,&High[0],&Low[0],Config);
    BuildAnalyzerFromPrice(B,SSE_DAILY_COUNT,&High[0],&Low[0],Config,&C);
    if (A.Structures.empty() || A.Candidates.empty() || B.Hierarchy.Nodes.empty() ||
        A.Structures.size()!=B.Structures.size() || A.Candidates.size()!=B.Candidates.size()) return false;
    std::vector<float> X(SSE_DAILY_COUNT),Y(SSE_DAILY_COUNT);
    for (int o=59;o<=91;o++)
    {
      // 所有slot都比较，包含落在同根的低优先级候选；不比较padding字节。
      int nSlots=static_cast<int>(o<=69 ? A.Structures.size() : A.Candidates.size());
      for (int slot=0;slot<nSlots;slot++)
      {
        if (o<=69)
        {
          ApplyNativeStructureProjection(SSE_DAILY_COUNT,&X[0],A,o,slot);
          ApplyNativeStructureProjection(SSE_DAILY_COUNT,&Y[0],B,o,slot);
        }
        else
        {
          ApplyNativeCandidateProjection(SSE_DAILY_COUNT,&X[0],A,o,slot);
          ApplyNativeCandidateProjection(SSE_DAILY_COUNT,&Y[0],B,o,slot);
        }
        if (std::memcmp(&X[0],&Y[0],X.size()*sizeof(float))!=0) return false;
      }
    }
  }
  return true;
}

bool TestLegacyIsolation()
{
  float Hi[17], Lo[17];
  for (int i=0;i<17;i++) { Hi[i]=H[i]; Lo[i]=L[i]; }
  CzscAnalyzer A, B;
  TrendAnchorContract C=Anchor(17);
  BuildAnalyzerFromPrice(A,17,Hi,Lo,DefaultConfig());
  BuildAnalyzerFromPrice(B,17,Hi,Lo,DefaultConfig(),&C);
  if (A.Structures.size()!=B.Structures.size() || A.Candidates.size()!=B.Candidates.size() ||
      B.Hierarchy.Nodes.empty()) return false;
  // 同进程旧投影在新增证据调用前后逐位一致；跨DLL对照另交管理者。
  float Before[59][17], After[17], Mode=0;
  for (int o=0;o<=58;o++) { Mode=static_cast<float>(o*10); Func30(17,Before[o],Hi,Lo,&Mode); }
  float Extended[21]={950,0,1,2};
  for (int i=0;i<17;i++) Extended[i+4]=200102;
  Func30(17,After,Hi,Lo,Extended);
  for (int o=0;o<=58;o++)
  {
    Mode=static_cast<float>(o*10); Func30(17,After,Hi,Lo,&Mode);
    if (std::memcmp(Before[o],After,sizeof(After))!=0) return false;
  }
  for (std::size_t i=0;i<A.Structures.size();i++)
  {
    const TrendStructure &X=A.Structures[i], &Y=B.Structures[i];
    if (X.nType!=Y.nType || X.nStart!=Y.nStart || X.nEnd!=Y.nEnd ||
        X.nFirstCenter!=Y.nFirstCenter || X.nLastCenter!=Y.nLastCenter) return false;
  }
  return true;
}

bool TestC4MovementsAndConnections()
{
  const float Hi[]={12,13,12,16,17,17,17,21,22,22,22,26,27};
  const float Lo[]={10,11,10,15,15,15,15,20,20,20,20,25,25};
  RecursiveMovementHierarchy R=BuildRecursiveMovements(13,Hi,Lo,Anchor(13));
  if (!R.bAvailable || R.Movements.size()!=1 || R.Centers.size()!=3) return false;
  const RecursiveMovement &M=R.Movements[0];
  if (M.nType!=1 || M.Centers.size()!=3 || M.Connections.size()!=2 ||
      M.nCompletedAt!=12 || M.nEnd!=11 || M.nLevel!=0) return false;
  for (std::size_t i=0;i<R.Connections.size();i++)
  {
    const RecursiveConnection &C=R.Connections[i];
    if (C.nLevel!=-1 || C.nMemberSpace!=0 || C.Members.size()!=3 ||
        C.Members.front()!=C.nStart || C.Members.back()!=C.nEnd) return false;
  }
  R=BuildRecursiveMovements(5,Hi,Lo,Anchor(5));
  if (R.Movements.size()!=1 || R.Movements[0].nType!=0 ||
      R.Movements[0].nCompletedAt!=4) return false; // 单枢即盘整，不取离开方向
  float DownH[13],DownL[13],TouchL[13];
  for (int i=0;i<13;i++) { DownH[i]=40-Lo[i]; DownL[i]=40-Hi[i]; TouchL[i]=Lo[i]; }
  R=BuildRecursiveMovements(13,DownH,DownL,Anchor(13));
  if (R.Movements.size()!=1 || R.Movements[0].nType!=-1) return false;
  for (int i=4;i<=6;i++) TouchL[i]=13;
  R=BuildRecursiveMovements(13,Hi,TouchL,Anchor(13));
  if (R.Movements.size()<2 || R.Movements[0].nType!=0) return false; // 波动区间相等拒绝同向
  R=BuildRecursiveMovements(12,Hi,Lo,Anchor(12));
  if(R.Movements[0].nType!=1 || R.Movements[0].nCompletedAt!=-1) return false;
  R=BuildRecursiveMovements(17,H,L,Anchor(17));
  bool parent=false, multiChild=false;
  for(std::size_t i=0;i<R.Centers.size();i++) if(R.Centers[i].nLevel>0)
  {
    parent=true;
    for(std::size_t j=0;j<R.Centers[i].Children.size();j++)
    {
      int id=R.Centers[i].Children[j];
      if(id<0 || static_cast<std::size_t>(id)>=R.Movements.size()) return false;
      const RecursiveMovement &Child=R.Movements[id];
      if(Child.nCompletedAt<0 || Child.nLevel!=R.Centers[i].nLevel-1) return false;
      if(Child.Centers.size()>1) multiChild=true;
    }
  }
  return parent && multiChild;
}

bool TestC4AssociationEvidence()
{
  const float Hi[]={12,13,12,16,17,17,17,21,22};
  const float Lo[]={10,11,10,15,15,15,15,20,20};
  RecursiveMovementHierarchy R=BuildRecursiveMovements(9,Hi,Lo,Anchor(9));
  if (R.Movements.size()!=1 || R.Centers.size()!=2) return false;
  std::vector<Center> C;
  for (std::size_t i=0;i<R.Centers.size();i++)
  {
    const SubTrendNode &N=R.Centers[i];
    float high=Hi[N.nCenterStart],low=Lo[N.nCenterStart];
    for(int j=N.nCenterStart;j<=N.nCenterEnd;j++)
    { high=std::max(high,Hi[j]); low=std::min(low,Lo[j]); }
    C.push_back(Center{N.nCenterStart,N.nCenterEnd,N.fCenterHigh,N.fCenterLow,high,low,1});
  }
  std::vector<TrendStructure> T(1,TrendStructure{1,0,6,0,1});
  TrendCompletionEvidence E={}; E.nTrend=0; E.nTrendSpace=0;
  E.nConnectionBar=7; E.nLatestBar=8;
  std::vector<TrendCompletionEvidence> Evidence(1,E);
  std::vector<StructureAssociation> A=BuildStructureAssociations(C,T,Evidence,R,Hi,Lo);
  if (A.size()!=1 || A[0].nStatus!=1 || A[0].nLevel!=0 ||
      A[0].nCompletion!=0 || A[0].Centers!=R.Movements[0].Centers) return false;
  C[0].fLow+=0.25f; // 时间完全相等仍不能映射价格不同中枢
  A=BuildStructureAssociations(C,T,Evidence,R,Hi,Lo);
  if(A[0].nStatus!=0 || A[0].nLevel!=-1) return false;
  C[0].fLow-=0.25f; Evidence[0].nLatestBar=7;
  if(BuildStructureAssociations(C,T,Evidence,R,Hi,Lo)[0].nStatus!=0) return false;
  Evidence[0]=E; R.Movements.push_back(R.Movements[0]);
  return BuildStructureAssociations(C,T,Evidence,R,Hi,Lo)[0].nStatus==2;
}

bool TestC4MonthlyAndExtendedAbi()
{
  TrendAnchorContract C;
  C.nVersion=3; C.Dates={20191231,20200131,20200228,20200331,20200430};
  if(!BuildRecursiveMovements(5,H,L,C).bAvailable) return false;
  C.Dates[2]=20200131;
  if(BuildRecursiveMovements(5,H,L,C).bAvailable) return false;
  C.Dates[2]=20200230;
  if(BuildRecursiveMovements(5,H,L,C).bAvailable) return false;
  float Hi[5],Lo[5],Out[5];
  for(int i=0;i<5;i++){Hi[i]=H[i];Lo[i]=L[i];}
  float Mode[]={-1000,0,0,3,191231,200131,200228,200331,200430};
  Func30(5,Out,Hi,Lo,Mode);
  if(Out[4]!=4) return false;
  for(int i=0;i<4;i++) if(Out[i]!=0) return false;
  Mode[0]=-11001000; // config1100 + output100，无100->config1碰撞
  Func30(5,Out,Hi,Lo,Mode); if(Out[4]!=4) return false;
  Mode[0]=-1040; Mode[2]=2;
  Func30(5,Out,Hi,Lo,Mode); if(Out[4]!=0) return false; // 单枢盘整
  Mode[2]=14; Func30(5,Out,Hi,Lo,Mode);
  for(int i=0;i<5;i++) if(Out[i]!=-1) return false;
  Mode[2]=0; Mode[1]=0.5f; Func30(5,Out,Hi,Lo,Mode);
  for(int i=0;i<5;i++) if(Out[i]!=-1) return false;
  return true;
}
}

// 可独立链接调用，避免既有dev断言提前返回时掩盖C4自检结果。
bool TestRecursiveMovementSuite()
{
  return TestC4MovementsAndConnections() && TestC4AssociationEvidence() &&
    TestC4MonthlyAndExtendedAbi();
}

bool TestTrendCompletionSuite()
{
  return TestAnchorContracts() && TestCompletionAndSuccessorSeparated() &&
    TestTouchReturnGapAndNoCenter() && TestStrictChildrenAndSnapshotIdDrift() &&
    TestLegacyGapsOverlapNoCandidates() && TestProjectionNoBackfillAndInvalidContract() &&
    TestAllEvidenceProjectionFields() && TestBollIndependentAuxiliary() &&
    TestLegacyIsolation() && TestSseMemberAndCandidateIsolation() &&
    TestRecursiveMovementSuite();
}
