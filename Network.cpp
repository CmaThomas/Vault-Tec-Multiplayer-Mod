#include "Network.h"

SingleResponse Network::CreateResponse(pDefault* packet, unsigned char priority, unsigned char reliability, unsigned char channel, vector<RakNetGUID> targets)
{
    vector<unsigned char> data = vector<unsigned char>();
    data.push_back(priority);
    data.push_back(reliability);
    data.push_back(channel);
    SingleResponse response = SingleResponse(pair<pDefault*, vector<unsigned char> >(packet, data), targets);
    return response;
}

SingleResponse Network::CreateResponse(pDefault* packet, unsigned char priority, unsigned char reliability, unsigned char channel, RakNetGUID target)
{
    vector<RakNetGUID> targets = vector<RakNetGUID>();
    targets.push_back(target);
    return CreateResponse(packet, priority, reliability, channel, targets);
}

NetworkResponse Network::CompleteResponse(SingleResponse response)
{
    NetworkResponse complete;
    complete.push_back(response);
    return complete;
}

void Network::Dispatch(RakPeerInterface* peer, NetworkResponse& response)
{
    if (peer == NULL)
        throw VaultException("RakPeerInterface is NULL");

    NetworkResponse::iterator it;
    vector<RakNetGUID>::iterator it2;

    for (it = response.begin(); it != response.end(); ++it)
    {
        for (it2 = it->second.begin(); it2 != it->second.end(); ++it2)
            peer->Send((char*) it->first.first->get(), it->first.first->length(), (PacketPriority) it->first.second.at(0), (PacketReliability) it->first.second.at(1), it->first.second.at(2), *it2, false);
        delete it->first.first;
    }
}