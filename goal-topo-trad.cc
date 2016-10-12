/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//         ----------     ---------- 
//  AP3 -- | Switch1 | --| Switch2 | -- H1
//         ----------     ----------
//   |      |       |          |
//   X     AP1     AP2         H2
//        | | |   | |||
//        X X |   X X|X
//            |      |
//            m2     m1  
//
// reference: http://blog.csdn.net/u012174021/article/details/42320033


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
//#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/log.h"
#include "ns3/olsr-helper.h"
// 给传统交换机(BridgeNetDevice)
#include "ns3/bridge-module.h"

#include "ns3/global-route-manager.h"

//包含 `gnuplot`和`Gnuplot2Ddatabase`
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"

#include "ns3/netanim-module.h"

#include <iostream>
#include <stdint.h>
#include <sstream>
#include <fstream>



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GoalTopo-Trad-Script");


bool tracing  = true;
ns3::Time timeout = ns3::Seconds (0);


double stopTime = 20.0;  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nHost       = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 4;
uint32_t nAp3Station = 1;


//std::string str_outputFileName = "goal-topo-trad.plt" ;
std::ofstream outputFileName("trace/goal-topo-trad.plt");  //GenerateOutput()接收的是ofstream类型的 
double nSamplingPeriod = 0.1;   // 抽样间隔，根据总的Simulation时间做相应的调整


/* for udp-server-client application. */
uint32_t nMaxPackets = 2000;    // The maximum packets to be sent.
double nInterval  = 0.01;  // The interval between two packet sent.

/* for tcp-bulk-send application. */   
uint32_t nMaxBytes = 0;  //Zero is unlimited.



bool
SetTimeout (std::string value)
{
  try {
      timeout = ns3::Seconds (atof (value.c_str ()));
      return true;
    }
  catch (...) { return false; }
  return false;
}


bool
CommandSetup (int argc, char **argv)
{

  CommandLine cmd;
  //cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  //cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  
  /* for udp-server-client application */
  cmd.AddValue ("MaxPackets", "The total packets available to be scheduled by the UDP application.", nMaxPackets);
  cmd.AddValue ("Interval", "The interval between two packet sent", nInterval);

  /* for tcp-bulk-send application. */
  
  //cmd.AddValue ("MaxBytes", "The amount of data to send in bytes", nMaxBytes);
  cmd.AddValue ("SamplingPeriod", "Sampling period", nSamplingPeriod);
  cmd.AddValue ("stopTime", "The time to stop", stopTime);
  
  cmd.Parse (argc, argv);
  return true;
}



/*
 * Calculate Throughput using Flowmonitor
 * 每个探针(probe)会根据四点来对包进行分类
 * -- when packet is `sent`;
 * -- when packet is `forwarded`;
 * -- when packet is `received`;
 * -- when packet is `dropped`;
 * 由于包是在IP层进行track的，所以任何的四层(TCP)重传的包，都会被认为是一个新的包
 */
void
CheckThroughput (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset)
{
  
  double localThrou = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  /* since fmhelper is a pointer, we should use it as a pointer.
   * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
   */
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
    /* 
     * `Ipv4FlowClassifier`
     * Classifies packets by looking at their IP and TCP/UDP headers. 
     * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
    */

    /* 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的 */
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    // `192.168.0.10`是client(Node #14)的IP, `192.168.0.2`是server(Node#6)的IP
    // `192.168.0.6` 是client(Node #10)的IP
    if ((t.sourceAddress=="192.168.0.6" && t.destinationAddress == "192.168.0.2"))
      {
          // UDP_PROT_NUMBER = 17
          std::cout << "Flow " << i->first  << "  Protocol  " << unsigned(t.protocol) << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "Time: " << Simulator::Now ().GetSeconds () << " s\n";
          std::cout << "Lost Packets = " << i->second.lostPackets << "\n";
          localThrou = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024 ;
          std::cout << "  Throughput: " <<  localThrou << " Mbps\n";
      }
     
      // 每迭代一次就把`时间`和`吞吐量`加入到dataset里面
      dataset.Add ((Simulator::Now ()).GetSeconds (), localThrou);

     }
  /* check throughput every nSamplingPeriod second(每隔nSamplingPeriod调用依次Simulation)
   * 表示每隔nSamplingPeriod时间
   */
  Simulator::Schedule (Seconds(nSamplingPeriod), 
    &CheckThroughput, fmhelper, monitor, dataset);
}

// By Joahannes Costa from GitHub
void DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset1){
  
  double delay = 0.0;
  monitor->CheckForLostPackets(); 
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats){ 
      //Ipv4FlowClassifier::FiveTuple fiveTuple = classifier->FindFlow (stats->first);
      delay = stats->second.delaySum.GetSeconds ();
      dataset1.Add((double)Simulator::Now().GetSeconds(), (double)delay);
    }
  
  Simulator::Schedule(Seconds(1), &DelayMonitor, fmhelper, monitor, dataset1);
}


int
main (int argc, char *argv[])
{

  #ifdef NS3_OPENFLOW
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  /* RTS/CTS 一种半双工的握手协议 */
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",UintegerValue (10));
  /* 设置最大WIFI覆盖距离为5m, 超出这个距离之后将无法传输WIFI信号 */
  //Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (5));
  
  /* 设置命令行参数 */
  CommandSetup (argc, argv) ;
  

  /*------- for gnuplot ------*/
  Gnuplot gnuplot;
  Gnuplot2dDataset dataset;
  //dataset->SetTitle ("Throughput VS Time");   // 这句好像不起什么作用
  dataset.SetStyle (Gnuplot2dDataset::LINES);

  /*----- init Helpers ----- */
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000));   // 100M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay
  
  /* 调用YansWifiChannelHelper::Default() 已经添加了默认的传播损耗模型, 下面不要再手动添加 */
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  ////////////////////////////////////////
  ////////////// LOSS MODEL //////////////
  ////////////////////////////////////////

  /* 
   * `FixedRssLossModel` will cause the `rss to be fixed` regardless
   * of the distance between the two stations, and the transmit power 
   *
   *
   *
   *
   *
   *
   */
  /* 传播延时速度是恒定的  */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* 很多地方都用这个，不知道什么意思  */
  // wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");  // !!! 加了这句之后AP和STA就无法连接了
  //wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  /* 不管发送功率是多少，都返回一个恒定的接收功率  */
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel (wifiChannel.Create());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  WifiHelper wifi;
  /* The SetRemoteStationManager method tells the helper the type of `rate control algorithm` to use. 
   * Here, it is asking the helper to use the AARF algorithm
   */
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  WifiMacHelper wifiMac;

 
  NS_LOG_INFO ("------------Creating Nodes------------");
  NodeContainer switchesNode, apsNode, hostsNode;
  NodeContainer staWifi1Nodes, staWifi2Nodes, staWifi3Nodes;

  switchesNode. Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  apsNode.      Create (nAp);        //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  hostsNode.    Create (nHost);      //2 Nodes(terminal1 and terminal2)-----node 5,6

  staWifi1Nodes.Create(nAp1Station);    // node 7,8,9
  staWifi2Nodes.Create(nAp2Station);    // node 10,11,12,13
  staWifi3Nodes.Create(nAp3Station);    //  node 14
  
  Ptr<Node> ap1WifiNode = apsNode.Get (0);
  Ptr<Node> ap2WifiNode = apsNode.Get (1);
  Ptr<Node> ap3WifiNode = apsNode.Get (2);
  

  /* 这里用一个`csmaNodes`来包括 `apsNode` 和 `hostsNode` */
  NodeContainer csmaNodes;
  csmaNodes.Add(apsNode);     // APs index : 0,1,2
  csmaNodes.Add(hostsNode);   // terminals index: 3,4 
  
  
  

  NS_LOG_INFO ("------------Creating Devices------------");
  /* CSMA Devices */
  //NetDeviceContainer csmaDevices; // 这个是不是多余的，不要行不行？
  NetDeviceContainer ap1CsmaDevice, ap2CsmaDevice, ap3CsmaDevice;
  NetDeviceContainer hostsDevice;
  NetDeviceContainer switch1Devices, switch2Devices;

  //csmaDevices = csma.Install (csmaNodes);  // 这个是不是多余的，不装行不行？
  //hostsDevice.Add (csmaDevices.Get(3));
  //hostsDevice.Add (csmaDevices.Get(4));
  
  /* WIFI Devices */
  NetDeviceContainer stasWifi1Device, apWifi1Device;
  NetDeviceContainer stasWifi2Device, apWifi2Device;
  NetDeviceContainer stasWifi3Device, apWifi3Device;

  
  NS_LOG_INFO ("------------Building Topology-------------");

  NetDeviceContainer link;


  /* #1 Connect traditional switch1 to traditional switch2 */
  link = csma.Install (NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Devices. Add(link.Get(0));
  switch2Devices. Add(link.Get(1));
  
  /* #2 Connect AP1, AP2 and AP3 to traditional switch1 ！！！*/  
  for (uint32_t i = 0; i < nAp; i++)
  {
    link = csma.Install (NodeContainer(apsNode.Get(i), switchesNode.Get(0)));   // Get(0) for switch1
    if (i == 0)
    {
      ap1CsmaDevice. Add(link.Get(0));
    }
    else if (i == 1)
    {
      ap2CsmaDevice. Add(link.Get(0));
    }
    else if (i == 2)
    {
      ap3CsmaDevice. Add(link.Get(0));
    }
    switch1Devices. Add(link.Get(1));
  }


  /* #3 Connect host1 and host2 to traditonal switch2  */
  for (uint32_t i = 0; i < nHost; i++)
  {
    link = csma.Install (NodeContainer(hostsNode.Get(i), switchesNode.Get(1)));   // Get(1) for switch2
    hostsDevice. Add(link.Get(0));
    switch2Devices. Add(link.Get(1));
  }

  Ptr<Node> switch1Node = switchesNode.Get(0);
  Ptr<Node> switch2Node = switchesNode.Get(1);

  
  NS_LOG_INFO ("----------Installing Bridge NetDevice----------");
  /*!!!!!!!!!!!! 关键的 BridgeHelper !!!!!!!!!!!*/
  BridgeHelper bridge1, bridge2;
  // 下面表示在某Node上安装BridgeNetDevice
  // This method creates an ns3::BridgeNetDevice, adds the device to the node
  bridge1. Install(switch1Node, switch1Devices);
  bridge2. Install(switch2Node, switch2Devices);

  //--------TODO
  BridgeHelper bridgeForAP1, bridgeForAP2, bridgeForAP3;
  bridgeForAP1. Install(apsNode.Get (0), NetDeviceContainer(apWifi1Device, ap1CsmaDevice));
  bridgeForAP2. Install(apsNode.Get (1), NetDeviceContainer(apWifi2Device, ap2CsmaDevice));
  bridgeForAP3. Install(apsNode.Get (2), NetDeviceContainer(apWifi3Device, ap3CsmaDevice));

  NS_LOG_INFO ("----------Configuring WIFI networks----------");
  Ssid ssid1 = Ssid ("ssid-AP1");
  Ssid ssid2 = Ssid ("ssid-AP2");
  Ssid ssid3 = Ssid ("ssid-AP3");
  //----------------------- Network AP1--------------------
  /* We want to make sure that our stations don't perform active probing.
   * (就是等AP发现STA，而STA不主动发现AP)
   */
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (true));
  stasWifi1Device = wifi.Install(wifiPhy, wifiMac, staWifi1Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  apWifi1Device   = wifi.Install(wifiPhy, wifiMac, ap1WifiNode);

  //----------------------- Network AP2--------------------
  /* We want to make sure that our stations don't perform active probing. */
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (true));
  stasWifi2Device = wifi.Install(wifiPhy, wifiMac, staWifi2Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  apWifi2Device   = wifi.Install(wifiPhy, wifiMac, ap2WifiNode);

  //----------------------- Network AP3--------------------
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (true));
  stasWifi3Device = wifi.Install(wifiPhy, wifiMac, staWifi3Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  apWifi3Device   = wifi.Install(wifiPhy, wifiMac, ap3WifiNode);

  MobilityHelper mobility;
  /* for staWifi--1--Nodes */
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (30),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (staWifi1Nodes);

  /* for staWifi--2--Nodes */
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (25),
    "MinY",      DoubleValue (30),
    "DeltaX",    DoubleValue (10),
    "DeltaY",    DoubleValue (10),
    "GridWidth", UintegerValue(2),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (staWifi2Nodes);
  

  /* for ConstantPosition Nodes */
  MobilityHelper mobility2;
  /* We want the AP to remain in a fixed position during the simulation 
   * only stations in AP1 and AP2 is mobile, the only station in AP3 is not mobile.
   */
  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility2.Install (apsNode);    // csmaNodes includes APs and terminals
  mobility2.Install (hostsNode);
  mobility2.Install (staWifi3Nodes);
  mobility2.Install (switchesNode);
  


  NS_LOG_INFO ("----------Installing Internet stack----------");

  //OlsrHelper olsr; // 没有olsr，吞吐量就是0，持续丢包！
  // Ipv4StaticRoutingHelper staticRoute;
  // Ipv4ListRoutingHelper list;

  // //---list.Add(,0/10); 其中0或者10代表priority
  // list.Add (staticRoute, 0);
  // list.Add (olsr, 10);


  /* Add internet stack to all the nodes, expect switches(交换机不用) */
  InternetStackHelper internet;
  //！internet.SetRoutingHelper (olsr); // has effect on the next Install ()

  internet.Install (apsNode);
  internet.Install (hostsNode);
  internet.Install (switchesNode); // 试一下装在交换机上行不行？虽然我看别人的代码都没有。测试知道发现不行，应该是只能装在有IP的Node上
  internet.Install (staWifi1Nodes);
  internet.Install (staWifi2Nodes);
  internet.Install (staWifi3Nodes);

  
  NS_LOG_INFO ("-----------Assigning IP Addresses.-----------");

  /* AP和 switch 不分配IP, 只把IP给 Host 和 STA */
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0");

  Ipv4InterfaceContainer h1h2Interface;
  Ipv4InterfaceContainer stasWifi1Interface;
  Ipv4InterfaceContainer stasWifi2Interface;
  Ipv4InterfaceContainer stasWifi3Interface;

  // AP2的地址池， WIFI和CSMA的
  Ipv4InterfaceContainer apWifi2Interface;
  Ipv4InterfaceContainer ap2CsmaInterface;

  h1h2Interface      = ip.Assign (hostsDevice);   // 192.168.0.1, 192.168.0.2
  // 共三个AP, 三个STA的 Interface
  stasWifi1Interface = ip.Assign (stasWifi1Device); // 192.168.0.3~5
  stasWifi2Interface = ip.Assign (stasWifi2Device); // 192.168.0.6~9
  stasWifi3Interface = ip.Assign (stasWifi3Device); // 192.168.0.10

  //!!!!看到打印出的路由表之后，看来还是要给AP分配IP啊(虽然可以暂时不要interface)，不然都无法路由
  ip.Assign (apWifi1Device);                        // 192.168.0.11
  apWifi2Interface = ip.Assign (apWifi2Device);     // 192.168.0.12
  ap2CsmaInterface = ip.Assign (ap2CsmaDevice);     // 192.168.0.13
  ip.Assign (apWifi3Device);                        // 192.168.0.14

  //Ipv4GlobalRoutingHelper::PopulateRoutingTables();


  NS_LOG_INFO ("-----------Enabling Static Routing.-----------");
  /* -----for StaticRouting(its very useful)----- */

  // Ptr<Ipv4> ap2Ip = apsNode.Get(1)->GetObject<Ipv4> ();
  // Ptr<Ipv4> h2Ip  = hostsNode.Get(1)->GetObject<Ipv4> ();

  // // for node 10
  // Ptr<Ipv4> sta1Wifi2Ip = staWifi2Nodes.Get(0)->GetObject<Ipv4> ();

  // /* the server  ---将 CSMA网络中的 H2 的默认下一跳为CSMA网络中的AP2 */
  // // for node 10
  // Ptr<Ipv4StaticRouting> h2StaticRouting = staticRoute.GetStaticRouting (h2Ip);
  // h2StaticRouting->SetDefaultRoute(ap2CsmaInterface.GetAddress(0), 1);
  
  // /* the client  ---将 WIFI#2 中的 STA1 的默认下一跳为其所在WIFI#2网络的AP2  */
  // // for node 10
  // Ptr<Ipv4StaticRouting> sta1Wifi2StaticRouting = staticRoute.GetStaticRouting (sta1Wifi2Ip);
  // sta1Wifi2StaticRouting->SetDefaultRoute(apWifi2Interface.GetAddress(0), 1);


  NS_LOG_INFO ("-----------Creating Applications.-----------");
  uint16_t port = 9;   // Discard port (RFC 863)
  
  

  /* UDP server */
  UdpServerHelper server (port);  // for the server side, only one param(port) is specified
  // for node 6
  ApplicationContainer serverApps = server.Install (hostsNode.Get(1));
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (Seconds(stopTime));  
  

  /* UDP client */
  UdpClientHelper client (h1h2Interface.GetAddress(1) ,port);
  client.SetAttribute ("MaxPackets", UintegerValue (nMaxPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds(nInterval)));  
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  // for node 14
  //ApplicationContainer clientApps = client.Install(staWifi3Nodes.Get(0));
  // for node 10
  ApplicationContainer clientApps = client.Install(staWifi2Nodes.Get(0));
  // for node 5
  //ApplicationContainer clientApps = client.Install(hostsNode.Get(0));
  clientApps.Start (Seconds(1.1));  
  clientApps.Stop (Seconds(stopTime));
  


  /*
  // TCP server
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (TimeValue(Seconds(stopTime)));


  // TCP client
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
  ApplicationContainer sourceApps = source.Install (staWifi3Nodes.Get(0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));
  */



  /** GlobalRouting does NOT work with Wi-Fi.
   * https://groups.google.com/forum/#!searchin/ns-3-users/wifi$20global$20routing/ns-3-users/Z9K1YrEmbcI/MrP2k47HAQAJ
   */

  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("-----------Configuring Tracing.-----------");

  /**
   * Configure tracing of all enqueue, dequeue, and NetDevice receive events.
   * Trace output will be sent to the file as below
   */
  if (tracing)
    {
      AsciiTraceHelper ascii;
      //csma.EnablePcapAll("goal-topo-trad");
      csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo-trad/goal-topo-trad.tr"));
      wifiPhy.EnablePcap ("goal-topo-trad/goal-topo-trad-ap1-wifi", apWifi1Device);
      wifiPhy.EnablePcap ("goal-topo-trad/goal-topo-trad-ap2-wifi", apWifi2Device);
      wifiPhy.EnablePcap ("goal-topo-trad/goal-topo-trad-ap2-sta1-wifi", stasWifi2Device);
      wifiPhy.EnablePcap ("goal-topo-trad/goal-topo-trad-ap3-wifi", apWifi3Device);
      wifiPhy.EnablePcap ("goal-topo-trad/goal-topo-trad-ap3-sta1-wifi", stasWifi3Device);
      // WifiMacHelper doesnot have `EnablePcap()` method
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-switch1-csma", switch1Devices);
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-switch2-csma", switch2Devices);
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-ap1-csma", ap1CsmaDevice);
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-ap2-csma", ap2CsmaDevice);
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-ap3-csma", ap3CsmaDevice);
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-H1-csma", hostsDevice.Get(0));
      csma.EnablePcap ("goal-topo-trad/goal-topo-trad-H2-csma", hostsDevice.Get(1));
    }

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     openflow-switch-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  // eg. tcpdump -nn -tt -r xxx.pcap
  //
  //csma.EnablePcapAll ("goal-topo-trad", false);

  AnimationInterface anim ("goal-topo-trad/goal-topo-trad.xml");
  anim.SetConstantPosition(switch1Node,30,0);             // s1-----node 0
  anim.SetConstantPosition(switch2Node,65,0);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),5,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),30,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),55,20);      // Ap3----node 4
  anim.SetConstantPosition(hostsNode.Get(0),65,20);    // H1-----node 5
  anim.SetConstantPosition(hostsNode.Get(1),75,20);    // H2-----node 6
  anim.SetConstantPosition(staWifi3Nodes.Get(0),55,40);  //   -----node 14

  anim.EnablePacketMetadata();   // to see the details of each packet




  NS_LOG_INFO ("------------Preparing for CheckThroughput.------------");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  /* 表示从第#1秒开始 */
  Simulator::Schedule(Seconds(1),&CheckThroughput, &flowmon, monitor, dataset);

  NS_LOG_INFO ("------------Running Simulation.------------");
  /* 以下的 Simulation::Stop() 和 Simulator::Run () 的顺序
   * 是根据 `ns3-lab-loaded-from-internet/lab1-task1-appelman.cc` 来的
   */
  Simulator::Stop (Seconds(stopTime));
  Simulator::Run ();

  // 测吞吐量
  CheckThroughput(&flowmon, monitor, dataset);   // 这句好像是多余的啊，随便，反正只加了一次调用


  // monitor->SerializeToXmlFile("trace/goal-topo-trad.flowmon", true, true);
  /* the SerializeToXmlFile () function 2nd and 3rd parameters 
   * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
   */



  Simulator::Destroy ();

  NS_LOG_INFO ("-------------Done.-------------");

  gnuplot.AddDataset (dataset);
  gnuplot.GenerateOutput (outputFileName);

  NS_LOG_INFO ("----------Added dataset to outputfile.----------");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is Not Enabled. Cannot Run Simulation.-----");
  #endif // NS3_OPENFLOW
}
