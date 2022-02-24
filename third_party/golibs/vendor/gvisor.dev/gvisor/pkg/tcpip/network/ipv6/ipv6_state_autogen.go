// automatically generated by stateify.

package ipv6

import (
	"gvisor.dev/gvisor/pkg/state"
)

func (i *icmpv6DestinationUnreachableSockError) StateTypeName() string {
	return "pkg/tcpip/network/ipv6.icmpv6DestinationUnreachableSockError"
}

func (i *icmpv6DestinationUnreachableSockError) StateFields() []string {
	return []string{}
}

func (i *icmpv6DestinationUnreachableSockError) beforeSave() {}

// +checklocksignore
func (i *icmpv6DestinationUnreachableSockError) StateSave(stateSinkObject state.Sink) {
	i.beforeSave()
}

func (i *icmpv6DestinationUnreachableSockError) afterLoad() {}

// +checklocksignore
func (i *icmpv6DestinationUnreachableSockError) StateLoad(stateSourceObject state.Source) {
}

func (i *icmpv6DestinationNetworkUnreachableSockError) StateTypeName() string {
	return "pkg/tcpip/network/ipv6.icmpv6DestinationNetworkUnreachableSockError"
}

func (i *icmpv6DestinationNetworkUnreachableSockError) StateFields() []string {
	return []string{
		"icmpv6DestinationUnreachableSockError",
	}
}

func (i *icmpv6DestinationNetworkUnreachableSockError) beforeSave() {}

// +checklocksignore
func (i *icmpv6DestinationNetworkUnreachableSockError) StateSave(stateSinkObject state.Sink) {
	i.beforeSave()
	stateSinkObject.Save(0, &i.icmpv6DestinationUnreachableSockError)
}

func (i *icmpv6DestinationNetworkUnreachableSockError) afterLoad() {}

// +checklocksignore
func (i *icmpv6DestinationNetworkUnreachableSockError) StateLoad(stateSourceObject state.Source) {
	stateSourceObject.Load(0, &i.icmpv6DestinationUnreachableSockError)
}

func (i *icmpv6DestinationPortUnreachableSockError) StateTypeName() string {
	return "pkg/tcpip/network/ipv6.icmpv6DestinationPortUnreachableSockError"
}

func (i *icmpv6DestinationPortUnreachableSockError) StateFields() []string {
	return []string{
		"icmpv6DestinationUnreachableSockError",
	}
}

func (i *icmpv6DestinationPortUnreachableSockError) beforeSave() {}

// +checklocksignore
func (i *icmpv6DestinationPortUnreachableSockError) StateSave(stateSinkObject state.Sink) {
	i.beforeSave()
	stateSinkObject.Save(0, &i.icmpv6DestinationUnreachableSockError)
}

func (i *icmpv6DestinationPortUnreachableSockError) afterLoad() {}

// +checklocksignore
func (i *icmpv6DestinationPortUnreachableSockError) StateLoad(stateSourceObject state.Source) {
	stateSourceObject.Load(0, &i.icmpv6DestinationUnreachableSockError)
}

func (i *icmpv6DestinationAddressUnreachableSockError) StateTypeName() string {
	return "pkg/tcpip/network/ipv6.icmpv6DestinationAddressUnreachableSockError"
}

func (i *icmpv6DestinationAddressUnreachableSockError) StateFields() []string {
	return []string{
		"icmpv6DestinationUnreachableSockError",
	}
}

func (i *icmpv6DestinationAddressUnreachableSockError) beforeSave() {}

// +checklocksignore
func (i *icmpv6DestinationAddressUnreachableSockError) StateSave(stateSinkObject state.Sink) {
	i.beforeSave()
	stateSinkObject.Save(0, &i.icmpv6DestinationUnreachableSockError)
}

func (i *icmpv6DestinationAddressUnreachableSockError) afterLoad() {}

// +checklocksignore
func (i *icmpv6DestinationAddressUnreachableSockError) StateLoad(stateSourceObject state.Source) {
	stateSourceObject.Load(0, &i.icmpv6DestinationUnreachableSockError)
}

func (e *icmpv6PacketTooBigSockError) StateTypeName() string {
	return "pkg/tcpip/network/ipv6.icmpv6PacketTooBigSockError"
}

func (e *icmpv6PacketTooBigSockError) StateFields() []string {
	return []string{
		"mtu",
	}
}

func (e *icmpv6PacketTooBigSockError) beforeSave() {}

// +checklocksignore
func (e *icmpv6PacketTooBigSockError) StateSave(stateSinkObject state.Sink) {
	e.beforeSave()
	stateSinkObject.Save(0, &e.mtu)
}

func (e *icmpv6PacketTooBigSockError) afterLoad() {}

// +checklocksignore
func (e *icmpv6PacketTooBigSockError) StateLoad(stateSourceObject state.Source) {
	stateSourceObject.Load(0, &e.mtu)
}

func init() {
	state.Register((*icmpv6DestinationUnreachableSockError)(nil))
	state.Register((*icmpv6DestinationNetworkUnreachableSockError)(nil))
	state.Register((*icmpv6DestinationPortUnreachableSockError)(nil))
	state.Register((*icmpv6DestinationAddressUnreachableSockError)(nil))
	state.Register((*icmpv6PacketTooBigSockError)(nil))
}
