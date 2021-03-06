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
//
//           ----Controller---
//            |             |
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

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/bridge-helper.h"
#include "ns3/olsr-helper.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"


#include "ns3/netanim-module.h"

using namespace ns3;

// 用于命令行操作 `$ export NS_LOG=GoalTopoScript=info`
NS_LOG_COMPONENT_DEFINE ("GoalTopoForMonitorTestScript");


bool verbose   = false;
bool use_drop  = false;
bool tracing   = false;
bool animation = false;


ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
  verbose = true;
  return true;
}

bool
SetDrop (std::string value)
{
  use_drop = true;
  return true;
}

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

int
main (int argc, char *argv[])
{
  uint32_t nAp         = 3;
  uint32_t nSwitch     = 2;
  uint32_t nTerminal   = 2;
  uint32_t nAp1Station = 3;
  uint32_t nAp2Station = 4;
  uint32_t nAp3Station = 1;

  ns3::Time stopTime = ns3::Seconds (5.0);

  #ifdef NS3_OPENFLOW


  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",UintegerValue (10));

  CommandLine cmd;
  cmd.AddValue ("nAp1Station", "Number of wifi STA devices of AP1", nAp1Station);
  cmd.AddValue ("nAp2Station", "Number of wifi STA devices of AP2", nAp2Station);
  cmd.AddValue ("nAp3Station", "Number of wifi STA devices of AP3", nAp3Station);

  cmd.AddValue ("v", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("verbose", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("d", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("drop", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  cmd.Parse (argc, argv);

  if (verbose)
    {
      // LogComponentEnable ("OpenFlowCsmaSwitch", LOG_LEVEL_INFO);
      // LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
      // LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
      // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);  
      // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO); 
    }
  

  //----- init Helpers -----
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000));   // 100M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel (wifiChannel.Create());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  WifiHelper wifi;
  //The SetRemoteStationManager method tells the helper the type of rate control algorithm to use. 
  //Here, it is asking the helper to use the AARF algorithm
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  WifiMacHelper wifiMac;

 
  NS_LOG_INFO ("-----Creating nodes-----");
 

  NodeContainer switchesNode;
  switchesNode.Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  
  NodeContainer apsNode;
  apsNode.Create (nAp);             //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  NodeContainer wifiAp1Node = apsNode.Get (0);
  NodeContainer wifiAp2Node = apsNode.Get (1);
  NodeContainer wifiAp3Node = apsNode.Get (2);

  NodeContainer terminalsNode;
  terminalsNode.Create (nTerminal); //2 Nodes(terminal1 and terminal2)-----node 5,6
  
  NodeContainer csmaNodes;
  csmaNodes.Add(apsNode);         // APs index : 0,1,2
  csmaNodes.Add(terminalsNode);   // terminals index: 3,4 

  

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Creating every  Ap's stations
  NodeContainer wifiAp1StaNodes;
  wifiAp1StaNodes.Create(nAp1Station);    // node 7,8,9

  NodeContainer wifiAp2StaNodes;
  wifiAp2StaNodes.Create(nAp2Station);    // node 10,11,12,13

  NodeContainer wifiAp3StaNodes;
  wifiAp3StaNodes.Create(nAp3Station);    //  node 14

  

  NS_LOG_INFO ("-----Building Topology------");

  // Create the csma links, from each AP & terminals to the switch
  NetDeviceContainer csmaAp1Device, csmaAp2Device, csmaAp3Device;
  csmaAp1Device.Add (csmaDevices.Get(0));
  csmaAp2Device.Add (csmaDevices.Get(1));
  csmaAp3Device.Add (csmaDevices.Get(2));

  NetDeviceContainer terminalsDevice;
  terminalsDevice.Add (csmaDevices.Get(3));
  terminalsDevice.Add (csmaDevices.Get(4));
  
  NetDeviceContainer switch1Device, switch2Device;
  NetDeviceContainer link;

  //Connect ofSwitch1 to ofSwitch2  
  link = csma.Install(NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));
  
  //Connect AP1, AP2 and AP3 to ofSwitch1  
  link = csma.Install(NodeContainer(csmaNodes.Get(0),switchesNode.Get(0)));
  // link is a list, including the two nodes
  // add one to apDevice{A,B,C}, the other to switch1Device
  csmaAp1Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(1),switchesNode.Get(0)));
  csmaAp2Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(2),switchesNode.Get(0)));
  csmaAp3Device.Add(link.Get(0));
  switch1Device.Add(link.Get(1));


  //Connect terminal1 and terminal2 to ofSwitch2  
  for (int i = 3; i < 5; i++)
    {
      link = csma.Install(NodeContainer(csmaNodes.Get(i), switchesNode.Get(1)));
      terminalsDevice.Add(link.Get(0));
      switch2Device.Add(link.Get(1));

    }


  //------- Network AP1-------
  NetDeviceContainer wifiSta1Device, wifiAp1Device;
  Ssid ssid1 = Ssid ("ssid-AP1");
  /*
  *  We want to make sure that our stations don't perform active probing.
  *  Finally, the “ActiveProbing” Attribute is set to `false`. 
  *  This means that probe requests will not be sent by MACs created by this helper. 
  *  "ns3::NqstaWifiMac" This means that the MAC will use a “non-QoS station” (nqsta) state machine.
  *  `BeaconGeneration`: Whether or not beacons are generated
  *  `BeaconInterval` : Delay between two beacons
  * */
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
  // 
  wifiSta1Device = wifi.Install(wifiPhy, wifiMac, wifiAp1StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  wifiAp1Device   = wifi.Install(wifiPhy, wifiMac, wifiAp1Node);    // csmaNodes

  //------- Network AP2-------
  NetDeviceContainer wifiSta2Device, wifiAp2Device;
  Ssid ssid2 = Ssid ("ssid-AP2");
  // We want to make sure that our stations don't perform active probing.
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
  wifiSta2Device = wifi.Install(wifiPhy, wifiMac, wifiAp2StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  wifiAp2Device   = wifi.Install(wifiPhy, wifiMac, wifiAp2Node);     // csmaNodes

  //------- Network AP3-------
  NetDeviceContainer wifiSta3Device, wifiAp3Device;
  Ssid ssid3 = Ssid ("ssid-AP3");
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (false));
  wifiSta3Device = wifi.Install(wifiPhy, wifiMac, wifiAp3StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  wifiAp3Device   = wifi.Install(wifiPhy, wifiMac, wifiAp3Node);    // csmaNodes

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiAp1StaNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (25),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiAp2StaNodes);
  

  MobilityHelper mobility2;
  // We want the AP to remain in a fixed position during the simulation
  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  // only stations in AP1 and AP2 is mobile, the only station in AP3 is not mobile.
  mobility2.Install (csmaNodes);    // csmaNodes includes APs and terminals
  mobility2.Install (wifiAp3StaNodes);
  mobility2.Install (switchesNode);

  //Create the switch netdevice,which will do the packet switching
  Ptr<Node> switchNode1 = switchesNode.Get (0);
  Ptr<Node> switchNode2 = switchesNode.Get (1);
  
  OpenFlowSwitchHelper switchHelper;

  if (use_drop)
    {
      Ptr<ns3::ofi::DropController> controller = CreateObject<ns3::ofi::DropController> ();
      switchHelper.Install (switchNode1, switch1Device, controller);
      switchHelper.Install (switchNode2, switch2Device, controller);
      //Ptr<ns3::ofi::DropController> controller2 = CreateObject<ns3::ofi::DropController> ();
      //switchHelper.Install (switchNode2, switch2Device, controller2);
    }
  else
    {
      Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode1, switch1Device, controller);
      //switchHelper.Install (switchNode2, switch2Device, controller);
      Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller2->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode2, switch2Device, controller2);
    }


  // We enable OLSR (which will be consulted at a higher priority than
  // the global routing) on the backbone nodes
  NS_LOG_INFO ("Enabling OLSR routing");
  OlsrHelper olsr;
  
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  Ipv4ListRoutingHelper list;
  list.Add (ipv4RoutingHelper, 0);
  list.Add (olsr, 10);

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (csmaNodes);
  internet.Install (wifiAp1StaNodes);
  internet.Install (wifiAp2StaNodes);
  internet.Install (wifiAp3StaNodes);

  NS_LOG_INFO ("-----Assigning IP Addresses.-----");

  Ipv4AddressHelper csmaIpAddress;
  csmaIpAddress.SetBase ("192.168.0.0", "255.255.255.0");

  // for Ap1,Ap2 and Ap3
  csmaIpAddress.Assign (csmaAp1Device);    // csmaDevices
  csmaIpAddress.Assign (csmaAp2Device); 
  //csmaIpAddress.Assign (csmaAp3Device);
  Ipv4InterfaceContainer csmaAp3Interface;
  csmaAp3Interface = csmaIpAddress.Assign (csmaAp3Device);
  Ipv4InterfaceContainer h1h2Interface;
  h1h2Interface = csmaIpAddress.Assign (terminalsDevice); 


  Ipv4AddressHelper ap1IpAddress;
  ap1IpAddress.SetBase ("10.0.1.0", "255.255.255.0");
  NetDeviceContainer wifi1Device = wifiSta1Device;
  wifi1Device.Add(wifiAp1Device);
  Ipv4InterfaceContainer interfaceA ;
  interfaceA = ap1IpAddress.Assign (wifi1Device);
  

  Ipv4AddressHelper ap2IpAddress;
  ap2IpAddress.SetBase ("10.0.2.0", "255.255.255.0");
  NetDeviceContainer wifi2Device = wifiSta2Device;
  wifi2Device.Add(wifiAp2Device);
  Ipv4InterfaceContainer interfaceB ;
  interfaceB = ap2IpAddress.Assign (wifi2Device);


  Ipv4AddressHelper ap3IpAddress;
  ap3IpAddress.SetBase ("10.0.3.0", "255.255.255.0");
  //NetDeviceContainer wifi3Device = wifiSta3Device;
  //wifi3Device.Add(wifiAp3Device);
  //Ipv4InterfaceContainer interfaceC ;
  //interfaceC = ap3IpAddress.Assign (wifi3Device);
  Ipv4InterfaceContainer apWifiInterfaceC ;
  Ipv4InterfaceContainer staWifiInterfaceC ;
  apWifiInterfaceC  = ap3IpAddress.Assign (wifiAp3Device);
  staWifiInterfaceC = ap3IpAddress.Assign (wifiSta3Device);


  /*
  // obtain a node pointer "node"
  Ipv4NatHelper natHelper ;
  Ptr<Ipv4Nat> nat = natHelper.Install (apsNode.Get (2));    //AP3
  // the Ipv4Interface indices are used to refer to the NAT interfaces
  nat->SetInside (1);
  nat->SetOutside (2);
  // NAT is configured to block all traffic that does not match one of the configured rules. 
  // The user would configure rules next, such as follows:
  // specify local and global IP addresses
  Ipv4StaticNatRule rule (Ipv4Address ("10.0.3.2"), Ipv4Address ("192.168.0.102"));
  nat->AddStaticRule (rule);

  // the following code will help printing the NAT rules to a file nat.rules from the stream:

  Ptr<OutputStreamWrapper> natStream = Create<OutputStreamWrapper> ("nat.rules", std::ios::out);
  nat->PrintTable (natStream);
  */

  // -----for StaticRouting(its very useful)-----
  
  Ptr<Ipv4> ipv4Ap3 = apsNode.Get(2)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4H2 = terminalsNode.Get(1)->GetObject<Ipv4> ();    // or csmaNodes.Get(4)
  Ptr<Ipv4> ipv4Ap3Sta = wifiAp3StaNodes.Get(0)->GetObject<Ipv4> ();    // node 14

  //Ipv4StaticRoutingHelper ipv4RoutingHelper;   // moved this code ahead
  // the intermedia AP3
  //Ptr<Ipv4StaticRouting> staticRoutingAp3 = ipv4RoutingHelper.GetStaticRouting (ipv4Ap3);
  //staticRoutingAp3->SetDefaultRoute(h1h2Interface.GetAddress(1), 1);
  //staticRoutingAp3->SetDefaultRoute(staWifiInterfaceC.GetAddress(0), 1);
  // the server
  Ptr<Ipv4StaticRouting> staticRoutingH2 = ipv4RoutingHelper.GetStaticRouting (ipv4H2);
  staticRoutingH2->SetDefaultRoute(csmaAp3Interface.GetAddress(0), 1);
  // the client
  Ptr<Ipv4StaticRouting> staticRoutingAp3Sta = ipv4RoutingHelper.GetStaticRouting (ipv4Ap3Sta);
  staticRoutingAp3Sta->SetDefaultRoute(apWifiInterfaceC.GetAddress(0), 1);
  

  // Add applications
  NS_LOG_INFO ("-----Creating Applications.-----");

/*
* Create a server application which waits for input UDP packets 
* and uses the information carried into their payload to compute delay 
* and to determine if some packets are lost. 
* 与`UdpEchoServerHelper`是不同的
*/

// Create one udpServer application on node one.
  uint16_t port1 = 8000;
  UdpEchoServerHelper server1 (port1);

  ApplicationContainer server_apps;
  server_apps = server1.Install (terminalsNode.Get(1));   // node #6 (192.168.0.8) (第二个固定终端节点) 作为 UdpServer

  server_apps.Start (Seconds (1.0));
  server_apps.Stop (Seconds (10.0));

//
// Create one UdpClient application to send UDP datagrams
//
  
  UdpEchoClientHelper client1 (h1h2Interface.GetAddress(1), port1);        // dest: IP,port

  client1.SetAttribute ("MaxPackets", UintegerValue (320));       // 最大数据包数
  client1.SetAttribute ("Interval", TimeValue (Time ("0.01")));   // 时间间隔
  client1.SetAttribute ("PacketSize", UintegerValue (1024));      // 包大小

  ApplicationContainer client_apps;
  client_apps = client1.Install (wifiAp1StaNodes.Get (2));    // node #9 (10.0.1.3)  也就是AP1下的9号节点

  client_apps.Start (Seconds (2.0));
  client_apps.Stop (Seconds (10.0));


  Simulator::Stop (Seconds(11.0));


  NS_LOG_INFO ("-----Configuring Tracing.-----");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file as below
  //
  if (tracing)
    {
      AsciiTraceHelper ascii;
      //csma.EnablePcapAll("goal-topo");
      csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo-for-monitor-test.tr"));
      wifiPhy.EnablePcap ("goal-topo-for-monitor-test-ap1-wifi", wifiAp1Device);
      wifiPhy.EnablePcap ("goal-topo-for-monitor-test-ap2-wifi", wifiAp2Device);
      wifiPhy.EnablePcap ("goal-topo-for-monitor-test-ap3-wifi", wifiAp3Device);
      wifiPhy.EnablePcap ("goal-topo-for-monitor-test-ap3Sta1-wifi", wifiSta3Device);
      // WifiMacHelper doesnot have `EnablePcap()` method
      csma.EnablePcap ("goal-topo-for-monitor-test-switch1-csma", switch1Device);
      csma.EnablePcap ("goal-topo-for-monitor-test-switch1-csma", switch2Device);
      csma.EnablePcap ("goal-topo-for-monitor-test-ap1-csma", csmaAp1Device);
      csma.EnablePcap ("goal-topo-for-monitor-test-ap2-csma", csmaAp2Device);
      csma.EnablePcap ("goal-topo-for-monitor-test-ap3-csma", csmaAp3Device);
      csma.EnablePcap ("goal-topo-for-monitor-test-H1-csma", terminalsDevice.Get(0));
      csma.EnablePcap ("goal-topo-for-monitor-test-H2-csma", terminalsDevice.Get(1));
    }

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     openflow-switch-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  // eg. tcpdump -nn -tt -r xxx.pcap
  //
  //csma.EnablePcapAll ("goal-topo", false);

  if (animation)
  {
    AnimationInterface anim ("goal-topo-for-monitor-test.xml");
    
    anim.SetConstantPosition(switchNode1,30,10);             // s1-----node 0
    anim.SetConstantPosition(switchNode2,65,10);             // s2-----node 1
    anim.SetConstantPosition(apsNode.Get(0),5,20);      // Ap1----node 2
    anim.SetConstantPosition(apsNode.Get(1),30,20);      // Ap2----node 3
    anim.SetConstantPosition(apsNode.Get(2),55,20);      // Ap3----node 4
    anim.SetConstantPosition(terminalsNode.Get(0),60,25);    // H1-----node 5
    anim.SetConstantPosition(terminalsNode.Get(1),65,25);    // H2-----node 6
    anim.SetConstantPosition(wifiAp3StaNodes.Get(0),55,30);  //   -----node 14

    anim.EnablePacketMetadata();   // to see the details of each packet

  }



/*
** Calculate Throughput using Flowmonitor
*/
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


/*
** Now, do the actual simulation.
*/
  NS_LOG_INFO ("Run Simulation.");


  

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      /* `Ipv4FlowClassifier`
       * Classifies packets by looking at their IP and TCP/UDP headers. 
       * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
      */

      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";   // 传输了多少字节
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";   // 收到了多少字节
      // 得出吞吐量(Throughput是多少)
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
     }


  monitor->SerializeToXmlFile("goal-topo-for-monitor-test.flowmon", true, true);
  // the SerializeToXmlFile () function 2nd and 3rd parameters 
  // are used respectively to activate/deactivate the histograms and the per-probe detailed stats.

  
  Simulator::Run ();
  Simulator::Destroy ();


  
  NS_LOG_INFO ("-----Done.-----");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-----");
  #endif // NS3_OPENFLOW
}
