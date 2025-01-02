#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

#include "ns3/opengym-module.h"

#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleOlsrExample");

FlowMonitorHelper flowmonHelper;
Ptr<FlowMonitor> monitor; // 전역 변수로 선언

std::map<std::string, double> CalculateFlowStats(Ptr<FlowMonitor> monitor) {
    monitor->CheckForLostPackets();

    // 플로우 통계 가져오기
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalThroughput = 0;
    double totalDelay = 0;
    uint32_t validDelayCount = 0;

    for (const auto& flow : stats) {
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;

        // Throughput 계산
        double throughput = flow.second.rxBytes * 8.0 / 20.0 / 1024 / 1024; // Mbps
        totalThroughput += throughput;

        // Delay 계산
        if (flow.second.rxPackets > 0) {
            double delay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            if (!std::isnan(delay)) {
                totalDelay += delay;
                validDelayCount++;
            }
        }
    }

    // 평균값 계산
    double avgPDR = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) * 100 : 0;
    double avgThroughput = (stats.size() > 0) ? (totalThroughput / stats.size()) : 0;
    double avgDelay = (validDelayCount > 0) ? (totalDelay / validDelayCount) : 0;

    // 결과 저장
    std::map<std::string, double> flowStats = {
        {"avgPDR", avgPDR},
        {"avgThroughput", avgThroughput},
        {"avgDelay", avgDelay}
    };

    return flowStats;
}

Ptr<OpenGymSpace> GetObservationSpace() {
// 상태 공간 정의 (예: 1개의 상태 변수, 범위 1~3)
std::vector<uint32_t> shape = {1}; // 차원 크기
Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(1.0, 3.0, shape, "float");
return space;
}

Ptr<OpenGymSpace> GetActionSpace() {
// 행동 공간 정의 (예: HelloInterval, TcInterval 값 설정 가능 범위)
std::vector<uint32_t> shape = {1}; // 차원 크기
Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(2.0, 5.0, shape, "float");
return space;
}

Ptr<OpenGymDataContainer> GetObservation() {
uint32_t totalQValue = 0;
uint32_t nodeCount = NodeList::GetNNodes(); // 전체 노드 수

for (uint32_t nodeId = 0; nodeId < nodeCount; ++nodeId) {
    Ptr<Node> node = NodeList::GetNode(nodeId); // 첫 번째 노드를 기준으로 사용
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting>(ipv4->GetRoutingProtocol());

    if (!listRouting) {
        NS_LOG_ERROR("Failed to retrieve Ipv4ListRouting instance.");
        Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>();
        box->AddValue(0.0); // 실패 시 기본 QValue 값
        return box;
    }

    Ptr<olsr::RoutingProtocol> olsrRouting = nullptr;

    // Ipv4ListRouting 내에서 OLSR 라우팅 프로토콜 검색
    for (uint32_t i = 0; i < listRouting->GetNRoutingProtocols(); ++i) {
        int16_t priority;
        Ptr<Ipv4RoutingProtocol> proto = listRouting->GetRoutingProtocol(i, priority);
        olsrRouting = DynamicCast<olsr::RoutingProtocol>(proto);
        if (olsrRouting) {
            break;
        }
    }

    if (!olsrRouting) {
        NS_LOG_ERROR("Failed to retrieve OLSR RoutingProtocol instance.");
        Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>();
        box->AddValue(0.0); // 실패 시 기본 QValue 값
        return box;
    }

    // OLSR 프로토콜에서 QValue 가져오기
    totalQValue = olsrRouting->GetQValue();
    // NS_LOG_INFO("QValue obtained: " << totalQValue);
}

// QValue를 OpenGym 상태로 반환
Ptr<OpenGymBoxContainer<float>> box = CreateObject<OpenGymBoxContainer<float>>();
if (box == nullptr) {
    NS_FATAL_ERROR("Failed to create OpenGymBoxContainer!");
}
box->AddValue(static_cast<float>(totalQValue)); // QValue를 float로 변환하여 반환
return box;
}

float GetReward() {
    // 플로우 통계 계산
    std::map<std::string, double> flowStats = CalculateFlowStats(monitor);

    double avgPDR = flowStats["avgPDR"];
    NS_LOG_INFO("평균 PDR: " << avgPDR);

    // 보상 조건 설정
    float reward = 0.0;
    float PDRset = 25;
    // 보상 조건 설정
    if (avgPDR >= PDRset) {
        reward = avgPDR/100; // PDR 증가에 따라 기하급수적 보상 증가
    }
    else{
        reward = 0;
    }
    NS_LOG_INFO("보상값: " << reward);
    return reward;
}

bool GetGameOver() {
// 시뮬레이션 종료 여부 반환
return false;  // 종료되지 않음
}

std::string GetExtraInfo() {
// 추가 정보 반환
return "Additional Info";
}

bool ExecuteActions(Ptr<OpenGymDataContainer> action) {
// 행동 적용 (예: HelloInterval 및 TcInterval 값 설정)
Ptr<OpenGymBoxContainer<float>> box = DynamicCast<OpenGymBoxContainer<float>>(action);
//float helloInterval = box->GetValue(0);
float tcInterval = box->GetValue(0);

// 첫 번째 노드에서 OLSR 라우팅 프로토콜 객체 가져오기
Ptr<Node> node = NodeList::GetNode(0); // 첫 번째 노드 기준
Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
Ptr<Ipv4ListRouting> listRouting = DynamicCast<Ipv4ListRouting>(ipv4->GetRoutingProtocol());

if (!listRouting) {
    NS_LOG_ERROR("Failed to retrieve Ipv4ListRouting instance.");
    return false;
}

Ptr<olsr::RoutingProtocol> olsrRouting = nullptr;

// Ipv4ListRouting 내에서 OLSR 라우팅 프로토콜 검색
for (uint32_t i = 0; i < listRouting->GetNRoutingProtocols(); ++i) {
    int16_t priority;
    Ptr<Ipv4RoutingProtocol> proto = listRouting->GetRoutingProtocol(i, priority);
    olsrRouting = DynamicCast<olsr::RoutingProtocol>(proto);
    if (olsrRouting) {
        break;
    }
}

if (!olsrRouting) {
    NS_LOG_ERROR("Failed to retrieve OLSR RoutingProtocol instance.");
    return false;
}
NS_LOG_INFO ("tc 조정:\t" << tcInterval);
//olsrRouting->SetHelloInterval(Seconds(helloInterval));
olsrRouting->SetTcInterval(Seconds(tcInterval));

//  NS_LOG_INFO("Updated HelloInterval to: " << helloInterval);
//  NS_LOG_INFO("Updated TcInterval to: " << tcInterval);

return true;  // 성공적으로 적용
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym){
Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
openGym->NotifyCurrentState();
}


#define ENABLE_INTERMEDIATE_LOGGING

void IntermediateRxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
#ifdef ENABLE_INTERMEDIATE_LOGGING
    NS_LOG_UNCOND("Intermediate node received one packet on interface " << interface << "!");
#endif
}

int main(int argc, char *argv[]){   
    
uint32_t simSeed = 8;           // 시뮬레이션 시드
double simulationTime = 30;     // 단위: seconds
double envStepTime = 0.1;       // 단위: seconds, ns3gym env step time interval
uint32_t openGymPort = 5555;    // 포트 번호
uint32_t testArg = 0;

// 로그 구역
LogComponentEnable("SimpleOlsrExample", LOG_LEVEL_INFO);
LogComponentDisable("PacketSink", LOG_LEVEL_INFO);          // PacketSink 로깅 활성화
LogComponentDisable("OnOffApplication", LOG_LEVEL_INFO);    // OnOff 로깅 비활성화
//LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_INFO);

    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("openGymPort", "OpenGym 포트 번호", openGymPort);
    cmd.AddValue("simSeed", "시뮬레이션 시드", simSeed);
    cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);
    cmd.Parse(argc, argv);

    //std::random_device rd; // 랜덤 시드 설정
    RngSeedManager::SetSeed (simSeed);
    RngSeedManager::SetRun (simSeed);

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(20); 

    // Set up WiFi devices and channel
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // 전파 손실 측정 // 로그 거리 전파 손실 모델 // 송신점에서의 거리의 로그에 비례하여 감소 // Exponent 경로 손실 지수(신호 감쇠 정도)
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(2));  // 일반적인 실외 환경 2~4

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::IdealWifiManager"); // Wi-Fi 속도 제어 // 이상적인 조건(채널 상태 정보 완벽) IdealWifiManager


    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    // Enable OLSR
    NS_LOG_INFO("Enabling OLSR Routing.");
    OlsrHelper olsr;

    Ipv4StaticRoutingHelper staticRouting;

    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(c);

    // Assign IP addresses
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // Set up mobility model
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0.0));
    x->SetAttribute("Max", DoubleValue(1000.0));
    Ptr<UniformRandomVariable> y = CreateObject<UniformRandomVariable>();
    y->SetAttribute("Min", DoubleValue(0.0));
    y->SetAttribute("Max", DoubleValue(1000.0));

    Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetX(x);
    positionAlloc->SetY(y);
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel", //속도
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=50.0|Max=50.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                            "PositionAllocator", PointerValue(positionAlloc));
    mobility.Install(c);

    NS_LOG_INFO("Create Applications.");
    uint16_t port = 9; // Discard port (RFC 863)

    // 노드 인덱스 벡터 생성 (0부터 n까지)
    std::vector<uint32_t> nodeIndices(c.GetN());
    std::iota(nodeIndices.begin(), nodeIndices.end(), 0);

    // 노드 인덱스를 섞습니다
    // std::mt19937 g(rd());
    // std::shuffle(nodeIndices.begin(), nodeIndices.end(), g);

    // 소스 노드와 목적지 노드를 선택합니다
    std::vector<uint32_t> sourceNodes(nodeIndices.begin(), nodeIndices.begin() + 6);  // 0 ~ n
    std::vector<uint32_t> destNodes(nodeIndices.begin() + 6, nodeIndices.begin() + 12);  // n ~ 2n

    // 각 소스 노드에 대해 OnOff 애플리케이션을 생성합니다
    for (size_t j = 0; j < sourceNodes.size(); ++j) {
        OnOffHelper onoff("ns3::UdpSocketFactory", 
                        InetSocketAddress(i.GetAddress(destNodes[j]), port));
        onoff.SetConstantRate(DataRate("80Kb/s"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer sourceApp = onoff.Install(c.Get(sourceNodes[j]));
        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(simulationTime));

        // 목적지 노드에 PacketSink 애플리케이션을 설치합니다
        PacketSinkHelper sink("ns3::UdpSocketFactory", 
                            InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(c.Get(destNodes[j]));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime+1.0));

        NS_LOG_INFO("Source node: " << sourceNodes[j] << " -> Destination node: " << destNodes[j]);
    }

    // FlowMonitor 설치
    // FlowMonitorHelper flowmonHelper;
    // Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    monitor = flowmonHelper.InstallAll();

    // OpenGymInterface 설정
    Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface>(openGymPort);
    openGym->SetGetObservationSpaceCb(MakeCallback(&GetObservationSpace));
    openGym->SetGetActionSpaceCb(MakeCallback(&GetActionSpace));
    openGym->SetGetObservationCb(MakeCallback(&GetObservation));
    openGym->SetGetRewardCb(MakeCallback(&GetReward));
    openGym->SetGetGameOverCb(MakeCallback(&GetGameOver));
    openGym->SetGetExtraInfoCb(MakeCallback(&GetExtraInfo));
    openGym->SetExecuteActionsCb(MakeCallback(&ExecuteActions));
    Simulator::Schedule (Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);

    NS_LOG_INFO ("시뮬레이션 시작");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    NS_LOG_INFO ("시뮬레이션 중단");

    // 시뮬레이션 후 결과 분석
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double avgThroughput = 0.0;
    double avgDelay = 0.0;
    double avgPacketDeliveryRatio = 0.0;
    uint32_t flowCount = 0;
    uint32_t validDelayCount = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i){
        double throughput = i->second.rxBytes * 8.0 / 20.0 / 1024 / 1024;
        double delay = 0;
        if (i->second.rxPackets > 0) {
            delay = i->second.delaySum.GetSeconds() / i->second.rxPackets;
            if (!std::isnan(delay)) {
                avgDelay += delay;
                validDelayCount++;
            }
        }
        double pdr = 0;
        if (i->second.txPackets > 0) {
            pdr = (double)i->second.rxPackets / i->second.txPackets * 100;
        }
        avgThroughput += throughput;
        avgPacketDeliveryRatio += pdr;
        flowCount++;
    }

    NS_LOG_UNCOND("\nAverage Results:");
    if (flowCount > 0) {
        avgThroughput /= flowCount;
        avgPacketDeliveryRatio /= flowCount;

        NS_LOG_UNCOND("Average Throughput = " << avgThroughput << " Mbps");
        NS_LOG_UNCOND("Average Packet Delivery Ratio = " << avgPacketDeliveryRatio << " %");
        
        if (validDelayCount > 0) {
            avgDelay /= validDelayCount;
            NS_LOG_UNCOND("Average End-to-End Delay = " << avgDelay << " s");
        }
    }

    openGym->NotifySimulationEnd();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
