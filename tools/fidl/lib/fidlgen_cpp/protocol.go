// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package fidlgen_cpp

import (
	"fmt"
	"strings"

	"go.fuchsia.dev/fuchsia/tools/fidl/lib/fidlgen"
)

const kMessageHeaderSize = 16

//
// Generate code for sending and receiving FIDL messages i.e. the messaging API.
//

// hlMessagingDetails represents the various generated definitions associated
// with a protocol, in the high-level C++ bindings.
// TODO(fxbug.dev/72798): Use the same approach to pass wireTypeNames and
// hlMessagingDetails to the templates.
type hlMessagingDetails struct {
	// ProtocolMarker is a pure-virtual interface corresponding to methods in
	// the protocol. Notably, HLCPP shares the same interface type between
	// the server and client bindings API.
	ProtocolMarker name

	// InterfaceAliasForStub is the type alias generated within the
	// "Stub" class, that refers to the pure-virtual interface corresponding to
	// the protocol.
	InterfaceAliasForStub name

	// Proxy implements the interface by encoding and making method calls.
	Proxy name

	// Stub calls into the interface after decoding an incoming message.
	// It also implements the EventSender interface.
	Stub name

	// EventSender is a pure-virtual interface for sending events.
	EventSender name

	// SyncInterface is a pure-virtual interface for making synchronous calls.
	SyncInterface name

	// SyncProxy implements the SyncInterface.
	SyncProxy name

	RequestEncoder  name
	RequestDecoder  name
	ResponseEncoder name
	ResponseDecoder name
}

func compileHlMessagingDetails(protocol nameVariants) hlMessagingDetails {
	p := protocol.HLCPP
	stub := p.appendName("_Stub")
	return hlMessagingDetails{
		ProtocolMarker:        p,
		InterfaceAliasForStub: stub.nest(p.appendName("_clazz").Name()),
		Proxy:                 p.appendName("_Proxy"),
		Stub:                  stub,
		EventSender:           p.appendName("_EventSender"),
		SyncInterface:         p.appendName("_Sync"),
		SyncProxy:             p.appendName("_SyncProxy"),
		RequestEncoder:        p.appendName("_RequestEncoder"),
		RequestDecoder:        p.appendName("_RequestDecoder"),
		ResponseEncoder:       p.appendName("_ResponseEncoder"),
		ResponseDecoder:       p.appendName("_ResponseDecoder"),
	}
}

type protocolWithHlMessaging struct {
	Protocol
	hlMessagingDetails
}

// WithHlMessaging returns a new protocol IR where the HLCPP bindings details
// are promoted to the same naming scope as the protocol. This makes it easier
// to access the HLCPP details in golang templates.
func (p Protocol) WithHlMessaging() protocolWithHlMessaging {
	return protocolWithHlMessaging{
		Protocol:           p,
		hlMessagingDetails: p.HlMessaging,
	}
}

// These correspond to templated classes and functions forward-declared in
// /src/lib/fidl/cpp/include/lib/fidl/cpp/unified_messaging.h
var (
	NaturalRequest     = fidlNs.member("Request")
	NaturalResponse    = fidlNs.member("Response")
	NaturalEvent       = fidlNs.member("Event")
	MessageTraits      = internalNs.member("MessageTraits")
	MessageBase        = internalNs.member("MessageBase")
	NaturalMethodTypes = internalNs.member("MethodTypes")

	// Client types
	NaturalClientImpl             = internalNs.member("NaturalClientImpl")
	NaturalClientCallbackTraits   = internalNs.member("ClientCallbackTraits")
	NaturalClientCallback         = fidlNs.member("ClientCallback")
	NaturalClientResponseCallback = fidlNs.member("ClientResponseCallback")
	NaturalAsyncEventHandler      = fidlNs.member("AsyncEventHandler")

	// NaturalEventHandlerInterface is shared between sync and async event handling.
	NaturalEventHandlerInterface = internalNs.member("NaturalEventHandlerInterface")
	NaturalEventDispatcher       = internalNs.member("NaturalEventDispatcher")

	// Server types
	NaturalWeakEventSender  = internalNs.member("NaturalWeakEventSender")
	NaturalEventSender      = internalNs.member("NaturalEventSender")
	NaturalServer           = fidlNs.member("Server")
	NaturalServerDispatcher = internalNs.member("NaturalServerDispatcher")
	NaturalCompleter        = internalNs.member("NaturalCompleter")
	NaturalCompleterBase    = internalNs.member("NaturalCompleterBase")
)

type unifiedMessagingDetails struct {
	NaturalClientImpl            name
	NaturalAsyncEventHandler     name
	NaturalEventHandlerInterface name
	NaturalEventDispatcher       name
	NaturalWeakEventSender       name
	NaturalEventSender           name
	NaturalServerDispatcher      name
	NaturalServer                name
}

func compileUnifiedMessagingDetails(protocol nameVariants, fidl fidlgen.Protocol) unifiedMessagingDetails {
	p := protocol.Wire
	return unifiedMessagingDetails{
		NaturalClientImpl:            NaturalClientImpl.template(p),
		NaturalAsyncEventHandler:     NaturalAsyncEventHandler.template(p),
		NaturalEventHandlerInterface: NaturalEventHandlerInterface.template(p),
		NaturalEventDispatcher:       NaturalEventDispatcher.template(p),
		NaturalWeakEventSender:       NaturalWeakEventSender.template(p),
		NaturalEventSender:           NaturalEventSender.template(p),
		NaturalServerDispatcher:      NaturalServerDispatcher.template(p),
		NaturalServer:                NaturalServer.template(p),
	}
}

// These correspond to templated classes forward-declared in
// //zircon/system/ulib/fidl/include/lib/fidl/llcpp/wire_messaging.h
var (
	// Protocol related
	WireSyncClient                 = fidlNs.member("WireSyncClient")
	WireClient                     = fidlNs.member("WireClient")
	WireEventHandlerInterface      = internalNs.member("WireEventHandlerInterface")
	WireSyncEventHandler           = fidlNs.member("WireSyncEventHandler")
	WireAsyncEventHandler          = transportNs.member("WireAsyncEventHandler")
	WireServer                     = transportNs.member("WireServer")
	WireEventSender                = internalNs.member("WireEventSender")
	InternalWireBufferEventSender  = internalNs.member("WireBufferEventSender")
	WireWeakEventSender            = internalNs.member("WireWeakEventSender")
	WireWeakBufferEventSender      = internalNs.member("WireWeakBufferEventSender")
	WireWeakOnewayClientImpl       = internalNs.member("WireWeakOnewayClientImpl")
	WireWeakAsyncClientImpl        = internalNs.member("WireWeakAsyncClientImpl")
	WireWeakAsyncBufferClientImpl  = internalNs.member("WireWeakAsyncBufferClientImpl")
	WireWeakOnewayBufferClientImpl = internalNs.member("WireWeakOnewayBufferClientImpl")
	WireWeakSyncClientImpl         = internalNs.member("WireWeakSyncClientImpl")
	WireSyncClientImpl             = internalNs.member("WireSyncClientImpl")
	WireSyncBufferClientImpl       = internalNs.member("WireSyncBufferClientImpl")
	WireEventDispatcher            = internalNs.member("WireEventDispatcher")
	WireServerDispatcher           = internalNs.member("WireServerDispatcher")

	// Method related
	TransactionalRequest    = internalNs.member("TransactionalRequest")
	TransactionalResponse   = internalNs.member("TransactionalResponse")
	TransactionalEvent      = internalNs.member("TransactionalEvent")
	WireRequest             = fidlNs.member("WireRequest")
	WireRequestView         = internalNs.member("WireRequestView")
	WireResponse            = fidlNs.member("WireResponse")
	WireEvent               = fidlNs.member("WireEvent")
	WireResult              = fidlNs.member("WireResult")
	WireUnownedResult       = transportNs.member("WireUnownedResult")
	WireResponseContext     = fidlNs.member("WireResponseContext")
	WireCompleter           = internalNs.member("WireCompleter")
	WireBufferCompleterImpl = internalNs.member("WireBufferCompleterImpl")
	WireCompleterImpl       = internalNs.member("WireCompleterImpl")
	WireCompleterBase       = internalNs.member("WireCompleterBase")
	WireMethodTypes         = internalNs.member("WireMethodTypes")
	WireOrdinal             = internalNs.member("WireOrdinal")
)

type wireTypeNames struct {
	// WireProtocolMarker is a class only used for containing other definitions
	// related to this protocol.
	// TODO(fxbug.dev/72798): Golang template should use this instead of the
	// nameVariants embedded in Protocol.
	WireProtocolMarker             name
	WireSyncClient                 name
	WireClient                     name
	WireEventHandlerInterface      name
	WireSyncEventHandler           name
	WireAsyncEventHandler          name
	WireServer                     name
	WireEventSender                name
	InternalWireBufferEventSender  name
	WireWeakEventSender            name
	WireWeakBufferEventSender      name
	WireWeakOnewayClientImpl       name
	WireWeakAsyncClientImpl        name
	WireWeakAsyncBufferClientImpl  name
	WireWeakOnewayBufferClientImpl name
	WireWeakSyncClientImpl         name
	WireSyncClientImpl             name
	WireSyncBufferClientImpl       name
	WireEventDispatcher            name
	WireServerDispatcher           name
}

func newWireTypeNames(protocolVariants nameVariants) wireTypeNames {
	p := protocolVariants.Wire
	return wireTypeNames{
		WireProtocolMarker:             p,
		WireSyncClient:                 WireSyncClient.template(p),
		WireClient:                     WireClient.template(p),
		WireEventHandlerInterface:      WireEventHandlerInterface.template(p),
		WireSyncEventHandler:           WireSyncEventHandler.template(p),
		WireAsyncEventHandler:          WireAsyncEventHandler.template(p),
		WireServer:                     WireServer.template(p),
		WireEventSender:                WireEventSender.template(p),
		InternalWireBufferEventSender:  InternalWireBufferEventSender.template(p),
		WireWeakEventSender:            WireWeakEventSender.template(p),
		WireWeakBufferEventSender:      WireWeakBufferEventSender.template(p),
		WireWeakOnewayClientImpl:       WireWeakOnewayClientImpl.template(p),
		WireWeakAsyncClientImpl:        WireWeakAsyncClientImpl.template(p),
		WireWeakAsyncBufferClientImpl:  WireWeakAsyncBufferClientImpl.template(p),
		WireWeakOnewayBufferClientImpl: WireWeakOnewayBufferClientImpl.template(p),
		WireWeakSyncClientImpl:         WireWeakSyncClientImpl.template(p),
		WireSyncClientImpl:             WireSyncClientImpl.template(p),
		WireSyncBufferClientImpl:       WireSyncBufferClientImpl.template(p),
		WireEventDispatcher:            WireEventDispatcher.template(p),
		WireServerDispatcher:           WireServerDispatcher.template(p),
	}
}

type Transport struct {
	Name          string
	Namespace     string
	Type          string
	HasEvents     bool
	HasSyncClient bool
}

var channelTransport = Transport{
	Name:          "Channel",
	Namespace:     "fidl",
	Type:          "::fidl::internal::ChannelTransport",
	HasEvents:     true,
	HasSyncClient: true,
}

var driverTransport = Transport{
	Name:          "Driver",
	Namespace:     "fdf",
	Type:          "::fidl::internal::DriverTransport",
	HasEvents:     false,
	HasSyncClient: true,
}

var transports = map[string]*Transport{
	"Channel": &channelTransport,
	"Driver":  &driverTransport,

	// Banjo and Syscall transports are skipped in templates, however they are
	// defined here to indicate they are known transports so that we can fail
	// on unknown and unhandled transports.
	"Banjo":   nil,
	"Syscall": nil,
}

// protocolInner contains information about a Protocol that should be
// filled out by the compiler.
type protocolInner struct {
	Attributes
	// TODO(fxbug.dev/72798): This should be replaced by ProtocolMarker in hlMessagingDetails
	// and wireMessagingDetails. In particular, the unified bindings do not declare
	// protocol marker classes.
	nameVariants

	// [Discoverable] protocols are exported to the outgoing namespace under this
	// name. This is deprecated by FTP-041 unified services.
	// TODO(fxbug.dev/8035): Remove.
	DiscoverableName string

	HlMessaging hlMessagingDetails
	unifiedMessagingDetails
	wireTypeNames

	// ClientAllocation is the allocation behavior of the client when receiving
	// FIDL events over this protocol.
	SyncEventAllocationV1 allocation
	SyncEventAllocationV2 allocation
	Methods               []Method
	FuzzingName           string
	DeprecatedTestBase    nameVariants
	TestBase              nameVariants
}

// Protocol should be created using newProtocol.
type Protocol struct {
	protocolInner

	// OneWayMethods contains the list of one-way (i.e. fire-and-forget) methods
	// in the protocol.
	OneWayMethods []*Method

	// TwoWayMethods contains the list of two-way (i.e. has both request and
	// response) methods in the protocol.
	TwoWayMethods []*Method

	// ClientMethods contains the list of client-initiated methods (i.e. any
	// interaction that is not an event). It is the union of one-way and two-way
	// methods.
	ClientMethods []*Method

	// Events contains the list of events (i.e. initiated by servers)
	// in the protocol.
	Events []*Method

	// Generated struct holding variant-agnostic details about protocol.
	ProtocolDetails name

	Transport *Transport
}

func (*Protocol) Kind() declKind {
	return Kinds.Protocol
}

var _ Kinded = (*Protocol)(nil)
var _ namespaced = (*Protocol)(nil)

func (p Protocol) HLCPPType() string {
	return p.HLCPP.String()
}

func (p Protocol) WireType() string {
	return p.Wire.String()
}

func newProtocol(inner protocolInner) Protocol {
	type kinds []methodKind

	filterBy := func(kinds kinds) []*Method {
		var out []*Method
		for i := 0; i < len(inner.Methods); i++ {
			m := &inner.Methods[i]
			k := m.methodKind()
			for _, want := range kinds {
				if want == k {
					out = append(out, m)
				}
			}
		}
		return out
	}

	return Protocol{
		protocolInner:   inner,
		OneWayMethods:   filterBy(kinds{oneWayMethod}),
		TwoWayMethods:   filterBy(kinds{twoWayMethod}),
		ClientMethods:   filterBy(kinds{oneWayMethod, twoWayMethod}),
		Events:          filterBy(kinds{eventMethod}),
		ProtocolDetails: makeName("fidl::internal::ProtocolDetails").template(inner.Wire),
	}
}

type argsWrapper []Parameter

// TODO(fxb/7704): We should be able to remove as we align with args with struct
// representation.
func (args argsWrapper) isResource() bool {
	for _, arg := range args {
		if arg.Type.IsResource {
			return true
		}
	}
	return false
}

type messageInner struct {
	TypeShapeV1     TypeShape
	TypeShapeV2     TypeShape
	HlCodingTable   *name
	WireCodingTable *name
}

// message contains lower level wire-format information about a request/response
// message.
// message should be created using newMessage.
type message struct {
	messageInner
	IsResource         bool
	ClientAllocationV1 allocation
	ClientAllocationV2 allocation
	ServerAllocationV1 allocation
	ServerAllocationV2 allocation
}

// methodContext indicates where the request/response is used.
// The allocation strategies differ for client and server contexts, in LLCPP.
type methodContext int

const (
	_ methodContext = iota
	clientContext
	serverContext
)

type boundednessQuery func(methodContext, fidlgen.Strictness) boundedness

func newMessage(inner messageInner, args []Parameter, wire wireTypeNames,
	direction messageDirection) message {
	return message{
		messageInner: inner,
		IsResource:   argsWrapper(args).isResource(),
		ClientAllocationV1: computeAllocation(
			inner.TypeShapeV1.MaxTotalSize(),
			direction.queryBoundedness(clientContext, inner.TypeShapeV1.HasFlexibleEnvelope)),
		ClientAllocationV2: computeAllocation(
			inner.TypeShapeV2.MaxTotalSize(),
			direction.queryBoundedness(clientContext, inner.TypeShapeV2.HasFlexibleEnvelope)),
		ServerAllocationV1: computeAllocation(
			inner.TypeShapeV1.MaxTotalSize(),
			direction.queryBoundedness(serverContext, inner.TypeShapeV1.HasFlexibleEnvelope)),
		ServerAllocationV2: computeAllocation(
			inner.TypeShapeV2.MaxTotalSize(),
			direction.queryBoundedness(serverContext, inner.TypeShapeV2.HasFlexibleEnvelope)),
	}
}

type wireMethod struct {
	WireCompleterAlias        name
	WireCompleter             name
	WireBufferCompleterImpl   name
	WireCompleterImpl         name
	WireCompleterBase         name
	WireMethodTypes           name
	WireOrdinal               name
	WireRequest               name
	WireRequestView           name
	WireRequestViewAlias      name
	WireEvent                 name
	WireResponse              name
	WireResponseContext       name
	WireTransactionalRequest  name
	WireTransactionalEvent    name
	WireTransactionalResponse name
	WireResult                name
	WireUnownedResult         name
}

func newWireMethod(name string, wireTypes wireTypeNames, protocolMarker name, methodMarker name) wireMethod {
	s := wireTypes.WireServer.nest(name)
	return wireMethod{
		WireCompleterAlias:        s.appendName("Completer"),
		WireCompleter:             WireCompleter.template(methodMarker),
		WireBufferCompleterImpl:   WireBufferCompleterImpl.template(methodMarker),
		WireCompleterImpl:         WireCompleterImpl.template(methodMarker),
		WireCompleterBase:         WireCompleterBase.template(methodMarker),
		WireMethodTypes:           WireMethodTypes.template(methodMarker),
		WireOrdinal:               WireOrdinal.template(methodMarker),
		WireRequest:               WireRequest.template(methodMarker),
		WireRequestView:           WireRequestView.template(methodMarker),
		WireRequestViewAlias:      s.appendName("RequestView"),
		WireEvent:                 WireEvent.template(methodMarker),
		WireResponse:              WireResponse.template(methodMarker),
		WireResponseContext:       WireResponseContext.template(methodMarker),
		WireTransactionalRequest:  TransactionalRequest.template(methodMarker),
		WireTransactionalEvent:    TransactionalEvent.template(methodMarker),
		WireTransactionalResponse: TransactionalResponse.template(methodMarker),
		WireResult:                WireResult.template(methodMarker),
		WireUnownedResult:         WireUnownedResult.template(methodMarker),
	}
}

type unifiedMethod struct {
	NaturalRequest             name
	RequestMessageTraits       name
	RequestMessageBase         name
	NaturalResponse            name
	ResponseMessageTraits      name
	ResponseMessageBase        name
	NaturalEvent               name
	EventMessageTraits         name
	EventMessageBase           name
	ClientCallbackTraits       name
	ClientResponseCallbackType name
	NaturalMethodTypes         name
	NaturalRequestAlias        name
	NaturalCompleterAlias      name
	NaturalCompleter           name
	NaturalCompleterBase       name
}

func newUnifiedMethod(methodMarker name, unifiedTypes unifiedMessagingDetails) unifiedMethod {
	naturalRequest := NaturalRequest.template(methodMarker)
	naturalResponse := NaturalResponse.template(methodMarker)
	naturalEvent := NaturalEvent.template(methodMarker)
	common := unifiedTypes.NaturalServer.nest(methodMarker.Self())
	return unifiedMethod{
		NaturalRequest:             naturalRequest,
		RequestMessageTraits:       MessageTraits.template(naturalRequest),
		RequestMessageBase:         MessageBase.template(naturalRequest),
		NaturalResponse:            naturalResponse,
		ResponseMessageTraits:      MessageTraits.template(naturalResponse),
		ResponseMessageBase:        MessageBase.template(naturalResponse),
		NaturalEvent:               naturalEvent,
		EventMessageTraits:         MessageTraits.template(naturalEvent),
		EventMessageBase:           MessageBase.template(naturalEvent),
		ClientCallbackTraits:       NaturalClientCallbackTraits.template(methodMarker),
		ClientResponseCallbackType: NaturalClientResponseCallback.template(methodMarker),
		NaturalMethodTypes:         NaturalMethodTypes.template(methodMarker),
		NaturalRequestAlias:        common.appendName("Request"),
		NaturalCompleterAlias:      common.appendName("Completer"),
		NaturalCompleter:           NaturalCompleter.template(methodMarker),
		NaturalCompleterBase:       NaturalCompleterBase.template(methodMarker),
	}
}

// methodInner contains information about a Method that should be filled out by
// the compiler.
type methodInner struct {
	protocolName nameVariants
	Marker       nameVariants
	wireMethod
	unifiedMethod
	baseCodingTableName string
	requestTypeShapeV1  TypeShape
	requestTypeShapeV2  TypeShape
	responseTypeShapeV1 TypeShape
	responseTypeShapeV2 TypeShape

	Attributes

	// FullyQualifiedName is the fully qualified name as defined by
	// https://fuchsia.dev/fuchsia-src/contribute/governance/rfcs/0043_documentation_comment_format?hl=en#fully-qualified-names
	FullyQualifiedName string

	nameVariants
	Ordinal                   uint64
	IsEvent                   bool
	HasRequest                bool
	HasRequestPayload         bool
	RequestPayload            nameVariants
	RequestArgs               []Parameter
	RequestLayout             fidlgen.Layout
	RequestAnonymousChildren  []ScopedLayout
	HasResponse               bool
	HasResponsePayload        bool
	ResponsePayload           nameVariants
	ResponseArgs              []Parameter
	ResponseLayout            fidlgen.Layout
	ResponseAnonymousChildren []ScopedLayout
	Transitional              bool
	Result                    *Result
}

// Method should be created using newMethod.
type Method struct {
	methodInner
	OrdinalName         nameVariants
	Request             message
	Response            message
	CallbackType        *nameVariants
	ResponseHandlerType string
	ResponderType       string
	// Protocol is a reference to the containing protocol, for the
	// convenience of golang templates.
	Protocol  *Protocol
	Transport *Transport
}

func (m Method) WireRequestViewArg() string {
	return m.appendName("RequestView").Name()
}

func (m Method) WireCompleterArg() string {
	return m.appendName("Completer").nest("Sync").Name()
}

func (m Method) NaturalCompleterArg() string {
	return m.appendName("Completer").nest("Sync").Name()
}

// CtsMethodAnnotation generates a comment containing information about the FIDL
// method that is covered by the C++ generated method. It is primarily meant
// to be parsed by machines, but can serve as human readable documentation too.
// For more information see fxbug.dev/84332.
func (m Method) CtsMethodAnnotation() string {
	// If the formatting of this comment needs to change, it should be done
	// with consulting the CTS team.
	return "// cts-coverage-fidl-name:" + m.FullyQualifiedName
}

type messageDirection int

const (
	_ messageDirection = iota
	messageDirectionRequest
	messageDirectionResponse
)

// Compute boundedness based on client/server, request/response, and strictness.
func (d messageDirection) queryBoundedness(c methodContext, hasFlexibleEnvelope bool) boundedness {
	switch d {
	case messageDirectionRequest:
		if c == clientContext {
			// Allocation is bounded when sending request from a client.
			return boundednessBounded
		} else {
			return boundedness(!hasFlexibleEnvelope)
		}
	case messageDirectionResponse:
		if c == serverContext {
			// Allocation is bounded when sending response from a server.
			return boundednessBounded
		} else {
			return boundedness(!hasFlexibleEnvelope)
		}
	}
	panic(fmt.Sprintf("unexpected message direction: %v", d))
}

// messageBodyCodingTableNames creates a name for an anonymous coding table of a certain type
// (request/response/event).
func messageBodyCodingTableName(ns *namespace, ct string, suffix string) *name {
	n := ns.member(ct).appendName(suffix)
	return &n
}

func newMethod(inner methodInner, hl hlMessagingDetails, wire wireTypeNames, p fidlgen.Protocol) Method {
	// Create generated names for the coding tables.
	var hlRequestCodingTable, wireRequestCodingTable, hlResponseCodingTable, wireResponseCodingTable *name
	hlCodingTableNamespace := hl.ProtocolMarker.Namespace().append("_internal")
	wireCodingTableNamespace := wire.WireProtocolMarker.Namespace()
	if inner.HasRequestPayload {
		hlRequestCodingTable = messageBodyCodingTableName(&hlCodingTableNamespace, codingTableName(inner.RequestLayout.Name), "Table")
		wireRequestCodingTable = messageBodyCodingTableName(&wireCodingTableNamespace, codingTableName(inner.RequestLayout.Name), "Table")
	}
	if inner.HasResponsePayload {
		hlResponseCodingTable = messageBodyCodingTableName(&hlCodingTableNamespace, codingTableName(inner.ResponseLayout.Name), "Table")
		wireResponseCodingTable = messageBodyCodingTableName(&wireCodingTableNamespace, codingTableName(inner.ResponseLayout.Name), "Table")
	}

	var callbackType *nameVariants = nil
	if inner.HasResponse {
		callbackName := inner.appendName("Callback")
		callbackName.Unified = NaturalClientCallback.template(inner.Marker.Wire)
		callbackType = &callbackName
	}
	ordinalName := fmt.Sprintf("k%s_%s_Ordinal",
		inner.protocolName.HLCPP.Name(), inner.HLCPP.Name())

	m := Method{
		methodInner: inner,
		OrdinalName: nameVariants{
			HLCPP:   inner.protocolName.HLCPP.Namespace().append("internal").member(ordinalName),
			Wire:    inner.protocolName.Wire.Namespace().member(ordinalName),
			Unified: inner.protocolName.Wire.Namespace().member(ordinalName),
		},
		Request: newMessage(messageInner{
			TypeShapeV1:     inner.requestTypeShapeV1,
			TypeShapeV2:     inner.requestTypeShapeV2,
			HlCodingTable:   hlRequestCodingTable,
			WireCodingTable: wireRequestCodingTable,
		}, inner.RequestArgs, wire, messageDirectionRequest),
		Response: newMessage(messageInner{
			TypeShapeV1:     inner.responseTypeShapeV1,
			TypeShapeV2:     inner.responseTypeShapeV2,
			HlCodingTable:   hlResponseCodingTable,
			WireCodingTable: wireResponseCodingTable,
		}, inner.ResponseArgs, wire, messageDirectionResponse),
		CallbackType: callbackType,
		ResponseHandlerType: fmt.Sprintf("%s_%s_ResponseHandler",
			inner.protocolName.HLCPP.Name(), inner.HLCPP.Name()),
		ResponderType: fmt.Sprintf("%s_%s_Responder",
			inner.protocolName.HLCPP.Name(), inner.HLCPP.Name()),
		Protocol: nil,
	}
	return m
}

type methodKind int

const (
	oneWayMethod = methodKind(iota)
	twoWayMethod
	eventMethod
)

func (m *Method) methodKind() methodKind {
	if m.HasRequest {
		if m.HasResponse {
			return twoWayMethod
		}
		return oneWayMethod
	}
	if !m.HasResponse {
		panic("A method should have at least either a request or a response")
	}
	return eventMethod
}

func (m *Method) CallbackWrapper() string {
	return "fit::function"
}

type Parameter struct {
	nameVariants
	Type              Type
	OffsetV1          int
	OffsetV2          int
	HandleInformation *HandleInformation
}

func (p Parameter) NameAndType() (string, Type) {
	return p.Name(), p.Type
}

var _ Member = (*Parameter)(nil)

func anyEventHasFlexibleEnvelope(methods []Method) bool {
	for _, m := range methods {
		if m.Response.TypeShapeV1.HasFlexibleEnvelope != m.Response.TypeShapeV2.HasFlexibleEnvelope {
			panic("expected type shape v1 and v2 flexible envelope values to be identical")
		}
		if !m.HasRequest && m.HasResponse && m.Response.TypeShapeV1.HasFlexibleEnvelope {
			return true
		}
	}
	return false
}

func (c *compiler) compileProtocol(p fidlgen.Protocol) *Protocol {
	protocolName := c.compileNameVariants(p.Name)
	codingTableName := codingTableName(p.Name)
	hlMessaging := compileHlMessagingDetails(protocolName)
	unifiedMessaging := compileUnifiedMessagingDetails(protocolName, p)
	wireTypeNames := newWireTypeNames(protocolName)

	methods := []Method{}
	for _, v := range p.Methods {
		name := methodNameContext.transform(v.Name)

		var result *Result
		if v.HasError {
			result = c.resultForUnion[v.ResultType.Identifier]
		}

		methodMarker := protocolName.nest(name.Wire.Name())

		var requestChildren []ScopedLayout
		var requestTypeShapeV1 fidlgen.TypeShape
		var requestTypeShapeV2 fidlgen.TypeShape
		var requestPayloadStruct fidlgen.Struct
		if v.RequestPayload != nil {
			requestTypeShapeV1 = v.RequestPayload.TypeShapeV1
			requestTypeShapeV2 = v.RequestPayload.TypeShapeV2
			if val, ok := c.messageBodyStructs[v.RequestPayload.Identifier]; ok {
				requestPayloadStruct = val
				requestChildren = c.anonymousChildren[toKey(val.NamingContext)]
			}
		}

		var responseChildren []ScopedLayout
		var responseTypeShapeV1 fidlgen.TypeShape
		var responseTypeShapeV2 fidlgen.TypeShape
		var responsePayloadStruct fidlgen.Struct
		if v.ResponsePayload != nil {
			responseTypeShapeV1 = v.ResponsePayload.TypeShapeV1
			responseTypeShapeV2 = v.ResponsePayload.TypeShapeV2
			if val, ok := c.messageBodyStructs[v.ResponsePayload.Identifier]; ok {
				responsePayloadStruct = val
				responseChildren = c.anonymousChildren[toKey(val.NamingContext)]
			}
		}

		var maybeRequestPayload nameVariants
		if v.HasRequestPayload() {
			maybeRequestPayload = c.compileNameVariants(v.RequestPayload.Identifier)
		}

		var maybeResponsePayload nameVariants
		if v.HasResponsePayload() {
			maybeResponsePayload = c.compileNameVariants(v.ResponsePayload.Identifier)
		}

		method := newMethod(methodInner{
			nameVariants: name,
			protocolName: protocolName,
			// Using the raw identifier v.Name instead of the name after
			// reserved words logic, since that's the behavior in fidlc.
			baseCodingTableName: codingTableName + string(v.Name),
			Marker:              methodMarker,
			requestTypeShapeV1:  TypeShape{requestTypeShapeV1},
			requestTypeShapeV2:  TypeShape{requestTypeShapeV2},
			responseTypeShapeV1: TypeShape{responseTypeShapeV1},
			responseTypeShapeV2: TypeShape{responseTypeShapeV2},
			wireMethod:          newWireMethod(name.Wire.Name(), wireTypeNames, protocolName.Wire, methodMarker.Wire),
			unifiedMethod:       newUnifiedMethod(methodMarker.Wire, unifiedMessaging),
			Attributes:          Attributes{v.Attributes},
			// TODO(fxbug.dev/84834): Use the functionality in //tools/fidl/lib/fidlgen/identifiers.go
			FullyQualifiedName:        fmt.Sprintf("%s.%s", p.Name, v.Name),
			Ordinal:                   v.Ordinal,
			IsEvent:                   !v.HasRequest && v.HasResponse,
			HasRequest:                v.HasRequest,
			HasRequestPayload:         v.HasRequestPayload(),
			RequestPayload:            maybeRequestPayload,
			RequestArgs:               c.compileParameterArray(requestPayloadStruct),
			RequestLayout:             requestPayloadStruct.Layout,
			RequestAnonymousChildren:  requestChildren,
			HasResponse:               v.HasResponse,
			HasResponsePayload:        v.HasResponsePayload(),
			ResponsePayload:           maybeResponsePayload,
			ResponseArgs:              c.compileParameterArray(responsePayloadStruct),
			ResponseLayout:            responsePayloadStruct.Layout,
			ResponseAnonymousChildren: responseChildren,
			Transitional:              v.IsTransitional(),
			Result:                    result,
		}, hlMessaging, wireTypeNames, p)
		methods = append(methods, method)
	}

	var maxResponseSizeV1 int
	var maxResponseSizeV2 int
	for _, method := range methods {
		if size := method.responseTypeShapeV1.MaxTotalSize(); size > maxResponseSizeV1 {
			maxResponseSizeV1 = size
		}
		if size := method.responseTypeShapeV2.MaxTotalSize(); size > maxResponseSizeV2 {
			maxResponseSizeV2 = size
		}
	}

	fuzzingName := strings.ReplaceAll(strings.ReplaceAll(string(p.Name), ".", "_"), "/", "_")
	testBaseNames := protocolName.appendName("_TestBase").appendNamespace("testing")
	testBaseNames.Wire = testingNs.member("WireTestBase").template(protocolName.Wire)
	r := newProtocol(protocolInner{
		Attributes:              Attributes{p.Attributes},
		nameVariants:            protocolName,
		HlMessaging:             hlMessaging,
		unifiedMessagingDetails: unifiedMessaging,
		wireTypeNames:           wireTypeNames,
		DiscoverableName:        p.GetServiceName(),
		SyncEventAllocationV1: computeAllocation(
			maxResponseSizeV1, messageDirectionResponse.queryBoundedness(
				clientContext, anyEventHasFlexibleEnvelope(methods))),
		SyncEventAllocationV2: computeAllocation(
			maxResponseSizeV2, messageDirectionResponse.queryBoundedness(
				clientContext, anyEventHasFlexibleEnvelope(methods))),
		Methods:     methods,
		FuzzingName: fuzzingName,
		TestBase:    testBaseNames,
	})
	var transport *Transport
	if len(p.Transports()) != 1 {
		panic("expected exactly one transport")
	}
	for t := range p.Transports() {
		var ok bool
		transport, ok = transports[t]
		if !ok {
			panic("transport not found")
		}
	}
	r.Transport = transport
	for i := 0; i < len(methods); i++ {
		methods[i].Protocol = &r
		methods[i].Transport = r.Transport
	}
	return &r
}

func (c *compiler) compileParameterArray(val fidlgen.Struct) []Parameter {
	var params []Parameter = []Parameter{}
	for _, v := range val.Members {
		params = append(params, Parameter{
			Type:              c.compileType(v.Type),
			nameVariants:      structMemberContext.transform(v.Name),
			OffsetV1:          v.FieldShapeV1.Offset,
			OffsetV2:          v.FieldShapeV2.Offset,
			HandleInformation: c.fieldHandleInformation(&v.Type),
		})
	}
	return params
}

//
// Functions for calculating message buffer size bounds
//

func fidlAlign(size int) int {
	return (size + 7) & ^7
}

type boundedness bool

const (
	boundednessBounded   = true
	boundednessUnbounded = false
)

// This value needs to be kept in sync with the one defined in
// zircon/system/ulib/fidl/include/lib/fidl/llcpp/sync_call.h
const llcppMaxStackAllocSize = 512
const channelMaxMessageSize = 65536

// allocation describes the allocation strategy of some operation, such as
// sending requests, receiving responses, or handling events. Note that the
// allocation strategy may depend on client/server context, direction of the
// message, and the content/shape of the message, as we make optimizations.
type allocation struct {
	IsStack bool
	Size    int

	bufferType bufferType
	size       string
}

func (alloc allocation) BackingBufferType() string {
	switch alloc.bufferType {
	case inlineBuffer:
		return fmt.Sprintf("::fidl::internal::InlineMessageBuffer<%s>", alloc.size)
	case boxedBuffer:
		return fmt.Sprintf("::fidl::internal::BoxedMessageBuffer<%s>", alloc.size)
	}
	panic(fmt.Sprintf("unexpected buffer type: %v", alloc.bufferType))
}

type bufferType int

const (
	_ bufferType = iota
	inlineBuffer
	boxedBuffer
)

func computeAllocation(maxTotalSize int, boundedness boundedness) allocation {
	var sizeString string
	var size int
	if boundedness == boundednessUnbounded || maxTotalSize > channelMaxMessageSize {
		sizeString = "ZX_CHANNEL_MAX_MSG_BYTES"
		size = channelMaxMessageSize
	} else {
		size = maxTotalSize + kMessageHeaderSize
		sizeString = fmt.Sprintf("%d", size)
	}

	if size > llcppMaxStackAllocSize {
		return allocation{
			IsStack:    false,
			Size:       0,
			bufferType: boxedBuffer,
			size:       sizeString,
		}
	} else {
		return allocation{
			IsStack:    true,
			Size:       size,
			bufferType: inlineBuffer,
			size:       sizeString,
		}
	}
}