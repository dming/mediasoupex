import { Logger } from '../Logger';
import { Channel } from '../Channel';
import { PayloadChannel } from '../PayloadChannel';
import { EnhancedEventEmitter } from '../EnhancedEventEmitter';
import { RtmpDirectTransportData } from './RtmpDirectTransport';
import { RtmpServerTransportData } from './RtmpServerTransport';

export type RtmpTransportInternal =
{
	rtmpTransportId: string;
};

export type TransportConstructorOptions =
{
	internal: RtmpTransportInternal;
	data: TransportData;
	channel: Channel;
	payloadChannel: PayloadChannel;
	appData?: Record<string, unknown>;
};

export type RtmpTransportEvents =
{ 
	rtmpRouterclose: [];
	// Private events.
	'@close': [];
};

type TransportData =
  | RtmpServerTransportData
  | RtmpDirectTransportData;

const logger = new Logger('RtmpTransport');

export class RtmpTransport<Events extends RtmpTransportEvents = RtmpTransportEvents> 
	extends EnhancedEventEmitter<Events>
{

	// Internal data.
	protected readonly internal: RtmpTransportInternal;

	// Channel instance.
	protected readonly channel: Channel;
	protected readonly payloadChannel: PayloadChannel;

	// Close flag.
	#closed = false;
    
	/**
	 * Transport id.
	 */
	get id(): string
	{
		return this.internal.rtmpTransportId;
	}

	/**
	 * Whether the Transport is closed.
	 */
	get closed(): boolean
	{
		return this.#closed;
	}

	constructor(
		{
			internal,
			channel,
			payloadChannel
		}: TransportConstructorOptions
	)
	{
		super();

		logger.debug('constructor() ');
		this.internal = internal;
		this.channel = channel;
		this.payloadChannel = payloadChannel;
	}

	/**
	 * Close the Transport.
	 */
	close(): void
	{
		if (this.#closed)
			return;

		logger.debug('close()');

		this.#closed = true;
	}
}