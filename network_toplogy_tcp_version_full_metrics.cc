#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>
#include <numeric>
#include <vector>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("top");

// Structure to hold network metrics
struct NetworkMetrics {
    double avgThroughput;
    double avgLatency;
    double jitter;
    double bandwidthUtilization;
    double rtt;
    uint32_t totalLostPackets;
    uint32_t totalRxPackets;
    uint32_t controlPackets;
    double networkOverhead;
    double contentRetrievalTime;
    std::vector<double> packetDelays;
};


void WriteEnhancedStatsToFile(std::string filename, const NetworkMetrics& metrics, double simulationTime) {
    std::ofstream outFile;
    outFile.open(filename.c_str(), std::ios::app);
    outFile << "\n=== Enhanced Network Statistics ===\n";
    outFile << "Average Throughput: " << metrics.avgThroughput << " Mbps\n";
    outFile << "Average Latency: " << metrics.avgLatency * 1000 << " ms\n";
    outFile << "Jitter: " << metrics.jitter * 1000 << " ms\n";
    outFile << "Bandwidth Utilization: " << metrics.bandwidthUtilization << "%\n";
    outFile << "Average RTT: " << metrics.rtt * 1000 << " ms\n";
    outFile << "Packet Loss Ratio: " << (double)metrics.totalLostPackets/(metrics.totalRxPackets + metrics.totalLostPackets) * 100 << "%\n";
    outFile << "Total Received Packets: " << metrics.totalRxPackets << "\n";
    outFile << "Total Lost Packets: " << metrics.totalLostPackets << "\n";
    outFile << "Control Packets: " << metrics.controlPackets << "\n";
    outFile << "Network Overhead: " << metrics.networkOverhead << "%\n";
    outFile << "Average Content Retrieval Time: " << metrics.contentRetrievalTime * 1000 << " ms\n";
    outFile << "Simulation Time: " << simulationTime << " seconds\n";
    outFile << "================================\n";
    outFile.close();
}


  // Calculate jitter from packet delays
  double CalculateJitter(const std::vector<double>& delays) {
    if (delays.size() < 2) return 0.0;
    
    std::vector<double> differences;
    for (size_t i = 1; i < delays.size(); ++i) {
        differences.push_back(std::abs(delays[i] - delays[i-1]));
    }
    
    return std::accumulate(differences.begin(), differences.end(), 0.0) / differences.size();
}

  // Calculate bandwidth utilization
  double CalculateBandwidthUtilization(uint64_t bytesReceived, double linkCapacity, double duration) {
    double actualThroughput = (bytesReceived * 8.0) / duration; // bits per second
    return (actualThroughput / (linkCapacity * 1e6)) * 100; // percentage
}


// Custom Client Application
class CustomClient : public Application {
private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendMessage(void);
    void HandleRead(Ptr<Socket> socket);
    void ScheduleTransmissions(void);
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    EventId m_sendEvent;
    uint32_t m_messageCount;
    std::string m_message;
    double m_interval; 

public:
    CustomClient();
    virtual ~CustomClient();
    void Setup(Address address, uint32_t packetSize);
};

CustomClient::CustomClient() : m_socket(0), m_peer(), m_packetSize(0), m_messageCount(0)
    , m_interval(1)  {
	m_message = "Message from client: Hello Server!";
}

CustomClient::~CustomClient() {
    m_socket = 0;
}

void CustomClient::Setup(Address address, uint32_t packetSize) {
    m_peer = address;
    m_packetSize = packetSize;
}

void CustomClient::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    m_socket->Bind();
    m_socket->Connect(m_peer);
    m_socket->SetRecvCallback(MakeCallback(&CustomClient::HandleRead, this));
    //SendMessage();
    ScheduleTransmissions();
}

void CustomClient::ScheduleTransmissions(void) {
    // Schedule first transmission
    Simulator::Schedule(Seconds(0.0), &CustomClient::SendMessage, this);
}

void CustomClient::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
    }
}

void CustomClient::SendMessage(void) {
    m_messageCount++;
    std::string message = "Hello from client i need !";
    // Add message number to the base message
    std::string numbered_message = m_message + " [" + std::to_string(m_messageCount) + "]";
    Ptr<Packet> packet = Create<Packet>((uint8_t*) message.c_str(), message.length());
    m_socket->Send(packet);
    //NS_LOG_INFO("Client " << GetNode()->GetId() << " sent: " << message);
     NS_LOG_INFO("Client " << GetNode()->GetId() 
                << " sent message " << m_messageCount 
                << " at time " << Simulator::Now().GetSeconds() 
                << "s: " << numbered_message);
      // Schedule next transmission if still running
    m_sendEvent = Simulator::Schedule(Seconds(m_interval), &CustomClient::SendMessage, this);           
}

void CustomClient::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        uint8_t buffer[1024];
        packet->CopyData(buffer, packet->GetSize());
        std::string received = std::string((char*)buffer, packet->GetSize());
        //NS_LOG_INFO("Client " << GetNode()->GetId() << " received: " << received);
        NS_LOG_INFO("Client " << GetNode()->GetId() 
                   << " received at time " << Simulator::Now().GetSeconds() 
                   << "s: " << received);
    }
}

// Custom Server Application
class CustomServer : public Application {
private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleAccept(Ptr<Socket> socket, const Address& from);
    void HandleRead(Ptr<Socket> socket);

    Ptr<Socket> m_socket;
    uint16_t m_port;
    uint32_t m_messagesReceived;

public:
    CustomServer();
    virtual ~CustomServer();
    void Setup(uint16_t port);
};

CustomServer::CustomServer() : m_socket(0), m_port(0), m_messagesReceived(0)  {}

CustomServer::~CustomServer() {
    m_socket = 0;
}

void CustomServer::Setup(uint16_t port) {
    m_port = port;
}

void CustomServer::StartApplication(void) {
    m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(
        MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
        MakeCallback(&CustomServer::HandleAccept, this));
    NS_LOG_INFO("Server started on node " << GetNode()->GetId());
}

void CustomServer::StopApplication(void) {
    if (m_socket) {
        m_socket->Close();
    }
}

void CustomServer::HandleAccept(Ptr<Socket> socket, const Address& from) {
    socket->SetRecvCallback(MakeCallback(&CustomServer::HandleRead, this));
    NS_LOG_INFO("Server accepted connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
}

void CustomServer::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        uint8_t buffer[1024];
        packet->CopyData(buffer, packet->GetSize());
        std::string received = std::string((char*)buffer, packet->GetSize());
        m_messagesReceived++;
        NS_LOG_INFO("Server received message " << m_messagesReceived 
                   << " at time " << Simulator::Now().GetSeconds() 
                   << "s: " << received);
        
        // Send response back to client
        std::string response = "Hello from server! do you want anything else"+ std::to_string(m_messagesReceived);
        Ptr<Packet> responsePacket = Create<Packet>((uint8_t*) response.c_str(), response.length());
        socket->Send(responsePacket);
        NS_LOG_INFO("Server sent response: " << response);
    }
}


int main(int argc, char *argv[])
{
    LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("top", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(26); 
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  
    // Create separate links
    NetDeviceContainer devices01, devices02, devices03; 
    NetDeviceContainer devices45, devices46;           
    NetDeviceContainer devices89, devices810;          
    NetDeviceContainer devices1112, devices1113, devices1114; 
    NetDeviceContainer devices1516;
    NetDeviceContainer devices1718;
    NetDeviceContainer devices1920;
    NetDeviceContainer devices2122;
    NetDeviceContainer devices2324;
    NetDeviceContainer devices70, devices74, devices78, devices711;
    NetDeviceContainer devices715, devices717, devices719, devices721, devices723;
    NetDeviceContainer devices250, devices254, devices258, devices2511;
    NetDeviceContainer devices2515, devices2517, devices2519, devices2521, devices2523;

    // Department connections
    devices01 = p2p.Install(nodes.Get(0), nodes.Get(1)); 
    devices02 = p2p.Install(nodes.Get(0), nodes.Get(2));
    devices03 = p2p.Install(nodes.Get(0), nodes.Get(3));
    devices45 = p2p.Install(nodes.Get(4), nodes.Get(5));
    devices46 = p2p.Install(nodes.Get(4), nodes.Get(6));
    devices89 = p2p.Install(nodes.Get(8), nodes.Get(9));
    devices810 = p2p.Install(nodes.Get(8), nodes.Get(10));
    devices1112 = p2p.Install(nodes.Get(11), nodes.Get(12));
    devices1113 = p2p.Install(nodes.Get(11), nodes.Get(13));
    devices1114 = p2p.Install(nodes.Get(11), nodes.Get(14));
    devices1516 = p2p.Install(nodes.Get(15), nodes.Get(16));
    devices1718 = p2p.Install(nodes.Get(17), nodes.Get(18));
    devices1920 = p2p.Install(nodes.Get(19), nodes.Get(20));
    devices2122 = p2p.Install(nodes.Get(21), nodes.Get(22));
    devices2324 = p2p.Install(nodes.Get(23), nodes.Get(24));
    
    // Multilayer switch connections node 7
    devices70 = p2p.Install(nodes.Get(7), nodes.Get(0));
    devices74 = p2p.Install(nodes.Get(7), nodes.Get(4));
    devices78 = p2p.Install(nodes.Get(7), nodes.Get(8));
    devices711 = p2p.Install(nodes.Get(7), nodes.Get(11));
    devices715 = p2p.Install(nodes.Get(7), nodes.Get(15));
    devices717 = p2p.Install(nodes.Get(7), nodes.Get(17));
    devices719 = p2p.Install(nodes.Get(7), nodes.Get(19));
    devices721 = p2p.Install(nodes.Get(7), nodes.Get(21));
    devices723 = p2p.Install(nodes.Get(7), nodes.Get(23));
    
    // Multilayer switch connections node 25
    devices250 = p2p.Install(nodes.Get(25), nodes.Get(0));
    devices254 = p2p.Install(nodes.Get(25), nodes.Get(4));
    devices258 = p2p.Install(nodes.Get(25), nodes.Get(8));
    devices2511 = p2p.Install(nodes.Get(25), nodes.Get(11));
    devices2515 = p2p.Install(nodes.Get(25), nodes.Get(15));
    devices2517 = p2p.Install(nodes.Get(25), nodes.Get(17));
    devices2519 = p2p.Install(nodes.Get(25), nodes.Get(19));
    devices2521 = p2p.Install(nodes.Get(25), nodes.Get(21));
    devices2523 = p2p.Install(nodes.Get(25), nodes.Get(23));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Flow Monitor installation
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();
  
    // IP address assignment
    Ipv4AddressHelper chaine_info_address;
    chaine_info_address.SetBase("192.168.10.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = chaine_info_address.Assign(devices01);
    Ipv4InterfaceContainer interfaces02 = chaine_info_address.Assign(devices02);
    Ipv4InterfaceContainer interfaces03 = chaine_info_address.Assign(devices03);
  
    Ipv4AddressHelper noyau_address;
    noyau_address.SetBase("192.168.20.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces45 = noyau_address.Assign(devices45);
    Ipv4InterfaceContainer interfaces46 = noyau_address.Assign(devices46);
     
    Ipv4AddressHelper fibreHome_address;
    fibreHome_address.SetBase("192.168.30.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces89 = fibreHome_address.Assign(devices89);
    Ipv4InterfaceContainer interfaces810 = fibreHome_address.Assign(devices810);
    
    Ipv4AddressHelper commutation_address;
    commutation_address.SetBase("192.168.40.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1112 = commutation_address.Assign(devices1112);
    Ipv4InterfaceContainer interfaces1113 = commutation_address.Assign(devices1113);
    Ipv4InterfaceContainer interfaces1114 = commutation_address.Assign(devices1114);

    Ipv4AddressHelper ericson_address;
    ericson_address.SetBase("192.168.50.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1516 = ericson_address.Assign(devices1516);
    
    Ipv4AddressHelper chaine_mecanique_address;
    chaine_mecanique_address.SetBase("192.168.60.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1718 = chaine_mecanique_address.Assign(devices1718);
    
    Ipv4AddressHelper finance_address;
    finance_address.SetBase("192.168.70.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1920 = finance_address.Assign(devices1920);

    Ipv4AddressHelper infermerie_address;
    infermerie_address.SetBase("192.168.80.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2122 = infermerie_address.Assign(devices2122);

    Ipv4AddressHelper pc_address;
    pc_address.SetBase("192.168.90.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2324 = pc_address.Assign(devices2324);

    Ipv4AddressHelper backbone1_address;
    backbone1_address.SetBase("192.168.100.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces70 = backbone1_address.Assign(devices70);
    Ipv4InterfaceContainer interfaces74 = backbone1_address.Assign(devices74);
    Ipv4InterfaceContainer interfaces78 = backbone1_address.Assign(devices78);
    Ipv4InterfaceContainer interfaces711 = backbone1_address.Assign(devices711);
    Ipv4InterfaceContainer interfaces715 = backbone1_address.Assign(devices715);
    Ipv4InterfaceContainer interfaces717 = backbone1_address.Assign(devices717);
    Ipv4InterfaceContainer interfaces719 = backbone1_address.Assign(devices719);
    Ipv4InterfaceContainer interfaces721 = backbone1_address.Assign(devices721);
    Ipv4InterfaceContainer interfaces723 = backbone1_address.Assign(devices723);
  
    Ipv4AddressHelper backbone2_address;
    backbone2_address.SetBase("192.168.110.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces250 = backbone2_address.Assign(devices250);
    Ipv4InterfaceContainer interfaces254 = backbone2_address.Assign(devices254);
    Ipv4InterfaceContainer interfaces258 = backbone2_address.Assign(devices258);
    Ipv4InterfaceContainer interfaces2511 = backbone2_address.Assign(devices2511);
    Ipv4InterfaceContainer interfaces2515 = backbone2_address.Assign(devices2515);
    Ipv4InterfaceContainer interfaces2517 = backbone2_address.Assign(devices2517);
    Ipv4InterfaceContainer interfaces2519 = backbone2_address.Assign(devices2519);
    Ipv4InterfaceContainer interfaces2521 = backbone2_address.Assign(devices2521);
    Ipv4InterfaceContainer interfaces2523 = backbone2_address.Assign(devices2523);  
    

    // TCP Server setup
    uint16_t port = 8080;
    Ptr<CustomServer> server = CreateObject<CustomServer>();
    server->Setup(port);
    nodes.Get(6)->AddApplication(server);
    server->SetStartTime(Seconds(1.0));
    server->SetStopTime(Seconds(10.0));

    // First TCP Client
    Ptr<CustomClient> client1 = CreateObject<CustomClient>();
    client1->Setup(InetSocketAddress(interfaces46.GetAddress(1), port), 1024);
    nodes.Get(22)->AddApplication(client1);
    client1->SetStartTime(Seconds(2.0));
    client1->SetStopTime(Seconds(10.0));

    // Second TCP Client
    //Ptr<CustomClient> client2 = CreateObject<CustomClient>();
    //client2->Setup(InetSocketAddress(interfaces46.GetAddress(1), port), 1024);
    //nodes.Get(24)->AddApplication(client2);
    //client2->SetStartTime(Seconds(3.0));
    //client2->SetStopTime(Seconds(20.0));

    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // Pcap captures
    p2p.EnablePcap("server", devices46.Get(1));
    p2p.EnablePcap("client1", devices2122.Get(1));
    //p2p.EnablePcap("client2", devices2324.Get(1));
  
    AnimationInterface anim("animation1.xml");
    
    // Node positions
    anim.SetConstantPosition(nodes.Get(7), 50.0, 20.0);
    anim.SetConstantPosition(nodes.Get(25), 118.0, 20.0);
    
    // Department switch positions
    anim.SetConstantPosition(nodes.Get(0), 5.0, 60.0);
    anim.SetConstantPosition(nodes.Get(4), 30.0, 60.0);
    anim.SetConstantPosition(nodes.Get(8), 50.0, 60.0);
    anim.SetConstantPosition(nodes.Get(11), 70.0, 60.0);
    anim.SetConstantPosition(nodes.Get(15), 90.0, 60.0);
    anim.SetConstantPosition(nodes.Get(17), 110.0, 60.0);
    anim.SetConstantPosition(nodes.Get(19), 130.0, 60.0);
    anim.SetConstantPosition(nodes.Get(21), 150.0, 60.0);
    anim.SetConstantPosition(nodes.Get(23), 170.0, 60.0);
    
    // End node positions
    anim.SetConstantPosition(nodes.Get(1), 0.0, 90.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 90.0);
    anim.SetConstantPosition(nodes.Get(3), 20.0, 90.0);
    anim.SetConstantPosition(nodes.Get(5), 25.0, 90.0);
    anim.SetConstantPosition(nodes.Get(6), 35.0, 90.0);
    anim.SetConstantPosition(nodes.Get(9), 45.0, 90.0);
    anim.SetConstantPosition(nodes.Get(10), 55.0, 90.0);
    anim.SetConstantPosition(nodes.Get(12), 65.0, 90.0);
    anim.SetConstantPosition(nodes.Get(13), 75.0, 90.0);
    anim.SetConstantPosition(nodes.Get(14), 85.0, 90.0);
    anim.SetConstantPosition(nodes.Get(16), 90.0, 90.0);
    anim.SetConstantPosition(nodes.Get(18), 110.0, 90.0);
    anim.SetConstantPosition(nodes.Get(20), 130.0, 90.0);
    anim.SetConstantPosition(nodes.Get(22), 150.0, 90.0);
    anim.SetConstantPosition(nodes.Get(24), 170.0, 90.0);

    // Node descriptions
    anim.UpdateNodeDescription(nodes.Get(7), "Multilayer1 ");
    anim.UpdateNodeDescription(nodes.Get(25), "Multilayer2 ");
    anim.UpdateNodeDescription(nodes.Get(0), "Chaine Info ");
    anim.UpdateNodeDescription(nodes.Get(4), "Noyau ");
    anim.UpdateNodeDescription(nodes.Get(8), "FibreHome ");
    anim.UpdateNodeDescription(nodes.Get(11), "Commutation ");
    anim.UpdateNodeDescription(nodes.Get(15), "Ericson ");
    anim.UpdateNodeDescription(nodes.Get(17), "Chaine mécanique ");
    anim.UpdateNodeDescription(nodes.Get(19), "Finance ");
    anim.UpdateNodeDescription(nodes.Get(21), "Infermerie ");
    anim.UpdateNodeDescription(nodes.Get(23), "PC ");
    
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();


    // Collect Enhanced Flow Statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();


    NetworkMetrics metrics;
    metrics.packetDelays.clear();
    double totalThroughput = 0.0;
    double totalLatency = 0.0;
    uint32_t flowCount = 0;
    uint64_t totalBytes = 0;
    double simulationTime = Simulator::Now().GetSeconds();

     // Open a file for detailed flow statistics
    std::ofstream detailedStats;
    detailedStats.open("detailed_enhanced_statistics.txt");
    detailedStats << "Detailed Enhanced Network Statistics\n";
    detailedStats << "===================================\n\n";

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        
        // Only consider flows from clients to server
        if (t.destinationAddress == interfaces46.GetAddress(1)) {
            flowCount++;
            
            // Calculate basic metrics
            double throughput = (i->second.rxBytes +i->second.txBytes)/ 
                             (i->second.timeLastRxPacket.GetSeconds() - 
                              i->second.timeFirstTxPacket.GetSeconds()) / 1000;
            
            double latency = i->second.delaySum.GetSeconds() / i->second.rxPackets;
            
            // Collect packet delays for jitter calculation
            if (i->second.rxPackets > 0) {
                metrics.packetDelays.push_back(i->second.delaySum.GetSeconds() / i->second.rxPackets);
            }
            
            // Accumulate totals
            totalThroughput += throughput;
            totalLatency += latency;
            totalBytes += i->second.rxBytes;
            
            // RTT calculation
            double rtt = i->second.delaySum.GetSeconds() / i->second.rxPackets * 2;
            
            // Write detailed statistics for each flow
            detailedStats << "Flow " << i->first << "\n";
            detailedStats << "Source: " << t.sourceAddress << "\n";
            detailedStats << "Destination: " << t.destinationAddress << "\n";
            detailedStats << "Throughput: " << throughput << " KBytes\n";
            detailedStats << "Latency: " << latency * 1000 << " ms\n";
            detailedStats << "RTT: " << rtt * 1000 << " ms\n";
            detailedStats << "Lost Packets: " << i->second.lostPackets << "\n";
            detailedStats << "Received Packets: " << i->second.rxPackets << "\n";
            detailedStats << "Control Packets: " << i->second.timesForwarded << "\n";
            detailedStats << "---------------------------\n\n";

            // Update metrics structure
            metrics.totalLostPackets += i->second.lostPackets;
            metrics.totalRxPackets += i->second.rxPackets;
            metrics.controlPackets += i->second.timesForwarded;
        }
    }
    
    detailedStats.close();


  // Calculate final metrics
    metrics.avgThroughput = totalThroughput / flowCount;
    metrics.avgLatency = totalLatency / flowCount;
    metrics.jitter = CalculateJitter(metrics.packetDelays);
    metrics.bandwidthUtilization = CalculateBandwidthUtilization(totalBytes, 100, simulationTime); // 100 Mbps link
    metrics.rtt = totalLatency * 2 / flowCount; // Approximate RTT
    metrics.networkOverhead = (double)metrics.controlPackets / (metrics.totalRxPackets +   metrics.controlPackets) * 100;
    metrics.contentRetrievalTime = totalLatency / flowCount; // Average time to retrieve content

    // Write enhanced statistics
    WriteEnhancedStatsToFile("enhanced_network_statistics.txt", metrics, simulationTime);

    Simulator::Destroy();
    return 0;
}

