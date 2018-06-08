/** @file beacon.h  Presence service based on UDP broadcasts.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_BEACON_H
#define LIBCORE_BEACON_H

#include "../Error"
#include "../Block"
#include "../Address"
#include "../Observers"

namespace de {

/**
 * UDP-based peer discovery mechanism.
 * @ingroup net
 */
class DE_PUBLIC Beacon
{
public:
    /// The UDP port was unavailable. @ingroup errors
    DE_ERROR(PortError);

    DE_DEFINE_AUDIENCE2(Discovery, void beaconFoundHost(const Address &host, const Block &message))
    DE_DEFINE_AUDIENCE2(Finished,  void beaconFinished())

public:
    Beacon(duint16 port);

    /**
     * Port the beacon uses for listening.
     * @return  Port.
     */
    duint16 port() const;

    /**
     * Starts the beacon with a message to give out.
     *
     * @param serviceListenPort
     *      TCP port that the advertised service listens on. Recipients will
     *      pair this with the IP address to form a full address.
     */
    void start(duint16 serviceListenPort);

    /**
     * Changes the message to advertise.
     *
     * @param advertisedMessage  Message to send to requesters.
     */
    void setMessage(IByteArray const &advertisedMessage);

    /**
     * Stops the beacon.
     */
    void stop();

    /**
     * Looks for any beacons on all accessible networks.
     *
     * @param timeOut   Maximum time to spend discovering. If the timeout
     *                  is zero or negative, discovery will not end.
     * @param interval  Interval between query broadcasts.
     */
    void discover(TimeSpan timeOut, TimeSpan interval = 1.0);

    List<Address> foundHosts() const;
    Block messageFromHost(Address const &host) const;

//protected slots:
//    void readIncoming();
//    void readDiscoveryReply();
//    void continueDiscovery();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_BEACON_H
