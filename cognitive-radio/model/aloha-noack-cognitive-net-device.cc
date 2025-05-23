
/*
 * Copyright (c) 2010 CTTC
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */
#include <iostream>
#include "aloha-noack-cognitive-net-device.h"
#include "ns3/aloha-noack-mac-header.h"

#include "ns3/boolean.h"
#include "ns3/channel.h"
#include "ns3/enum.h"
#include "ns3/llc-snap-header.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AlohaNoackCognitiveNetDevice");

/**
 * \brief Output stream operator
 * \param os output stream
 * \param state the state to print
 * \return an output stream
 */

std::ostream&
operator<<(std::ostream& os, AlohaNoackCognitiveNetDevice::State state)
{
    switch (state)
    {
    case AlohaNoackCognitiveNetDevice::IDLE:
        os << "IDLE";
        break;
    case AlohaNoackCognitiveNetDevice::TX:
        os << "TX";
        break;
    case AlohaNoackCognitiveNetDevice::RX:
        os << "RX";
        break;
    }
    return os;
}

NS_OBJECT_ENSURE_REGISTERED(AlohaNoackCognitiveNetDevice);

TypeId

AlohaNoackCognitiveNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AlohaNoackCognitiveNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Spectrum")
            .AddConstructor<AlohaNoackCognitiveNetDevice>()
            .AddAttribute("Address",
                          "The MAC address of this device.",
                          Mac48AddressValue(Mac48Address("12:34:56:78:90:12")),
                          MakeMac48AddressAccessor(&AlohaNoackCognitiveNetDevice::m_address),
                          MakeMac48AddressChecker())
            .AddAttribute("Queue",
                          "packets being transmitted get queued here",
                          PointerValue(),
                          MakePointerAccessor(&AlohaNoackCognitiveNetDevice::m_queue),
                          MakePointerChecker<Queue<Packet>>())
            .AddAttribute(
                "Mtu",
                "The Maximum Transmission Unit",
                UintegerValue(1500),
                MakeUintegerAccessor(&AlohaNoackCognitiveNetDevice::SetMtu, &AlohaNoackCognitiveNetDevice::GetMtu),
                MakeUintegerChecker<uint16_t>(1, 65535))
            .AddAttribute(
                "Phy",
                "The PHY layer attached to this device.",
                PointerValue(),
                MakePointerAccessor(&AlohaNoackCognitiveNetDevice::GetPhy, &AlohaNoackCognitiveNetDevice::SetPhy),
                MakePointerChecker<Object>())
            .AddTraceSource("MacTx",
                            "Trace source indicating a packet has arrived "
                            "for transmission by this device",
                            MakeTraceSourceAccessor(&AlohaNoackCognitiveNetDevice::m_macTxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MacTxDrop",
                            "Trace source indicating a packet has been dropped "
                            "by the device before transmission",
                            MakeTraceSourceAccessor(&AlohaNoackCognitiveNetDevice::m_macTxDropTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MacPromiscRx",
                            "A packet has been received by this device, has been "
                            "passed up from the physical layer "
                            "and is being forwarded up the local protocol stack.  "
                            "This is a promiscuous trace,",
                            MakeTraceSourceAccessor(&AlohaNoackCognitiveNetDevice::m_macPromiscRxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MacRx",
                            "A packet has been received by this device, "
                            "has been passed up from the physical layer "
                            "and is being forwarded up the local protocol stack.  "
                            "This is a non-promiscuous trace,",
                            MakeTraceSourceAccessor(&AlohaNoackCognitiveNetDevice::m_macRxTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}

AlohaNoackCognitiveNetDevice::AlohaNoackCognitiveNetDevice()
    : m_state(IDLE)
{
    NS_LOG_FUNCTION(this);
}

AlohaNoackCognitiveNetDevice::~AlohaNoackCognitiveNetDevice()
{
    NS_LOG_FUNCTION(this);
    m_queue = nullptr;
}

void

AlohaNoackCognitiveNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_queue = nullptr;
    m_node = nullptr;
    m_channel = nullptr;
    m_currentPkt = nullptr;
    m_phy = nullptr;
    m_phyMacTxStartCallback = MakeNullCallback<bool, Ptr<Packet>>();
    NetDevice::DoDispose();
}

void

AlohaNoackCognitiveNetDevice::SetIfIndex(const uint32_t index)
{
    NS_LOG_FUNCTION(index);
    m_ifIndex = index;
}

uint32_t

AlohaNoackCognitiveNetDevice::GetIfIndex() const
{
    NS_LOG_FUNCTION(this);
    return m_ifIndex;
}

bool

AlohaNoackCognitiveNetDevice::SetMtu(uint16_t mtu)
{
    NS_LOG_FUNCTION(mtu);
    m_mtu = mtu;
    return true;
}

uint16_t

AlohaNoackCognitiveNetDevice::GetMtu() const
{
    NS_LOG_FUNCTION(this);
    return m_mtu;
}

void

AlohaNoackCognitiveNetDevice::SetQueue(Ptr<Queue<Packet>> q)
{
    NS_LOG_FUNCTION(q);
    m_queue = q;
}

void

AlohaNoackCognitiveNetDevice::SetAddress(Address address)
{
    NS_LOG_FUNCTION(this);
    m_address = Mac48Address::ConvertFrom(address);
}

Address

AlohaNoackCognitiveNetDevice::GetAddress() const
{
    NS_LOG_FUNCTION(this);
    return m_address;
}

bool

AlohaNoackCognitiveNetDevice::IsBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address

AlohaNoackCognitiveNetDevice::GetBroadcast() const
{
    NS_LOG_FUNCTION(this);
    return Mac48Address::GetBroadcast();
}

bool

AlohaNoackCognitiveNetDevice::IsMulticast() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

Address

AlohaNoackCognitiveNetDevice::GetMulticast(Ipv4Address addr) const
{
    NS_LOG_FUNCTION(addr);
    Mac48Address ad = Mac48Address::GetMulticast(addr);
    return ad;
}

Address

AlohaNoackCognitiveNetDevice::GetMulticast(Ipv6Address addr) const
{
    NS_LOG_FUNCTION(addr);
    Mac48Address ad = Mac48Address::GetMulticast(addr);
    return ad;
}

bool

AlohaNoackCognitiveNetDevice::IsPointToPoint() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

bool

AlohaNoackCognitiveNetDevice::IsBridge() const
{
    NS_LOG_FUNCTION(this);
    return false;
}

Ptr<Node>

AlohaNoackCognitiveNetDevice::GetNode() const
{
    NS_LOG_FUNCTION(this);
    return m_node;
}

void

AlohaNoackCognitiveNetDevice::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(node);

    m_node = node;
}

void

AlohaNoackCognitiveNetDevice::SetPhy(Ptr<Object> phy)
{
    NS_LOG_FUNCTION(this << phy);
    m_phy = phy;
}

Ptr<Object>

AlohaNoackCognitiveNetDevice::GetPhy() const
{
    NS_LOG_FUNCTION(this);
    return m_phy;
}

void

AlohaNoackCognitiveNetDevice::SetChannel(Ptr<Channel> c)
{
    NS_LOG_FUNCTION(this << c);
    m_channel = c;
}

Ptr<Channel>

AlohaNoackCognitiveNetDevice::GetChannel() const
{
    NS_LOG_FUNCTION(this);
    return m_channel;
}

bool

AlohaNoackCognitiveNetDevice::NeedsArp() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

bool

AlohaNoackCognitiveNetDevice::IsLinkUp() const
{
    NS_LOG_FUNCTION(this);
    return m_linkUp;
}

void

AlohaNoackCognitiveNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    NS_LOG_FUNCTION(&callback);
    m_linkChangeCallbacks.ConnectWithoutContext(callback);
}

void

AlohaNoackCognitiveNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    NS_LOG_FUNCTION(&cb);
    m_rxCallback = cb;
}

void

AlohaNoackCognitiveNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb)
{
    NS_LOG_FUNCTION(&cb);
    m_promiscRxCallback = cb;
}

bool

AlohaNoackCognitiveNetDevice::SupportsSendFrom() const
{
    NS_LOG_FUNCTION(this);
    return true;
}

bool

AlohaNoackCognitiveNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(packet << dest << protocolNumber);
    return SendFrom(packet, m_address, dest, protocolNumber);
}

bool

AlohaNoackCognitiveNetDevice::SendFrom(Ptr<Packet> packet,
                              const Address& src,
                              const Address& dest,
                              uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(packet << src << dest << protocolNumber);

    LlcSnapHeader llc;
    llc.SetType(protocolNumber);
    packet->AddHeader(llc);

    AlohaNoackMacHeader header;
    header.SetSource(Mac48Address::ConvertFrom(src));
    header.SetDestination(Mac48Address::ConvertFrom(dest));
    packet->AddHeader(header);

    m_macTxTrace(packet);

    bool sendOk = true;
    //
    // If the device is idle, transmission starts immediately. Otherwise,
    // the transmission will be started by NotifyTransmissionEnd
    //
    NS_LOG_LOGIC(this << " state=" << m_state);
    if (m_state == IDLE)
    {
        if (m_queue->IsEmpty())
        {
            NS_LOG_LOGIC("new packet is head of queue, starting TX immediately");
            m_currentPkt = packet;
            StartTransmission();
        }
        else
        {
            NS_LOG_LOGIC("enqueueing new packet");
            if (!m_queue->Enqueue(packet))
            {
                m_macTxDropTrace(packet);
                sendOk = false;
            }
        }
    }
    else
    {
        NS_LOG_LOGIC("deferring TX, enqueueing new packet");
        NS_ASSERT(m_queue);
        if (!m_queue->Enqueue(packet))
        {
            m_macTxDropTrace(packet);
            sendOk = false;
        }
    }
    return sendOk;
}

void

AlohaNoackCognitiveNetDevice::SetGenericPhyTxStartCallback(GenericPhyTxStartCallback c)
{
    NS_LOG_FUNCTION(this);
    m_phyMacTxStartCallback = c;
}

void

AlohaNoackCognitiveNetDevice::StartTransmission()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_currentPkt);
    NS_ASSERT(m_state == IDLE);

    if (m_phyMacTxStartCallback(m_currentPkt))
    {
        NS_LOG_WARN("PHY refused to start TX");
    }
    else
    {
        m_state = TX;
    }
}

void

AlohaNoackCognitiveNetDevice::NotifyTransmissionEnd(Ptr<const Packet>)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_state == TX, "TX end notified while state != TX");
    m_state = IDLE;
    NS_ASSERT(m_queue);
    if (!m_queue->IsEmpty())
    {
        Ptr<Packet> p = m_queue->Dequeue();
        NS_ASSERT(p);
        m_currentPkt = p;
        NS_LOG_LOGIC("scheduling transmission now");
        Simulator::ScheduleNow(&AlohaNoackCognitiveNetDevice::StartTransmission, this);
    }
}

void

AlohaNoackCognitiveNetDevice::NotifyReceptionStart()
{
    NS_LOG_FUNCTION(this);
}

void

AlohaNoackCognitiveNetDevice::NotifyReceptionEndError()
{
    NS_LOG_FUNCTION(this);
}

void

AlohaNoackCognitiveNetDevice::NotifyReceptionEndOk(Ptr<Packet> packet)
{
    
    NS_LOG_FUNCTION(this << packet);
    AlohaNoackMacHeader header;
    packet->RemoveHeader(header);
    NS_LOG_LOGIC("packet " << header.GetSource() << " --> " << header.GetDestination()
                           << " (here: " << m_address << ")");

    LlcSnapHeader llc;
    packet->RemoveHeader(llc);

    PacketType packetType;
    if (header.GetDestination().IsBroadcast())
    {
        packetType = PACKET_BROADCAST;
    }
    else if (header.GetDestination().IsGroup())
    {
        packetType = PACKET_MULTICAST;
    }
    else if (header.GetDestination() == m_address)
    {
        packetType = PACKET_HOST;
    }
    else
    {
        packetType = PACKET_OTHERHOST;
    }

    NS_LOG_LOGIC("packet type = " << packetType);

    if (!m_promiscRxCallback.IsNull())
    {
        m_promiscRxCallback(this,
                            packet->Copy(),
                            llc.GetType(),
                            header.GetSource(),
                            header.GetDestination(),
                            packetType);
    }

    if (packetType != PACKET_OTHERHOST)
    {
        m_macRxTrace(packet);
        TotalNumberOfPackets++;
        m_rxCallback(this, packet, llc.GetType(), header.GetSource());
    }
}
uint32_t 
AlohaNoackCognitiveNetDevice:: GetTotalReceivedPackets()
{
    return TotalNumberOfPackets;
}

} // namespace ns3