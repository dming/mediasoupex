import { Logger } from './Logger';
import { Channel } from './Channel';
import { EnhancedEventEmitter } from './EnhancedEventEmitter';

export type RtmpServerListenInfo =
{
	/**
	 * Listening IPv4 or IPv6.
	 */
	listenIp: string;

	/**
	 * Announced IPv4 or IPv6 (useful when running mediasoup behind NAT with
	 * private IP).
	 */
	announcedIp?: string;

	/**
	 * Listening port.
	 */
	port?: number;
};

export type RtmpServerOptions =
{
	/**
	 * Listen infos.
	 */
	listenInfo: RtmpServerListenInfo;

	/**
	 * Custom application data.
	 */
	appData?: Record<string, unknown>;
};

export type RtmpServerEvents =
{ 
	workerclose: [];
	// Private events.
	'@close': [];
};

type RtmpServerInternal =
{
	rtmpServerId: string;
};

const logger = new Logger('RtmpServer');

export class RtmpServer extends EnhancedEventEmitter<RtmpServerEvents>
{
	// Internal data.
	readonly #internal: RtmpServerInternal;

	// Channel instance.
	readonly #channel: Channel;

	// Closed flag.
	#closed = false;

	// Custom app data.
	readonly #appData: Record<string, unknown>;

	constructor(
		{
			internal,
			channel,
			appData
		}:
		{
			internal: RtmpServerInternal;
			channel: Channel;
			appData?: Record<string, unknown>;
		}
	)
	{
		super();
		logger.debug('constructor()');

		this.#internal = internal;
		this.#channel = channel;
		this.#appData = appData || {};
	}

	/**
	 * Close the WebRtcServer.
	 */
	close(): void
	{
		if (this.#closed)
			return;
	
		logger.debug('close()');
	
		this.#closed = true;
	
		const reqData = { rtmpServerId: this.#internal.rtmpServerId };
	
		this.#channel.request('worker.closeRtmpServer', undefined, reqData)
			.catch(() => {});
	
		// // Close every WebRtcTransport.
		// for (const webRtcTransport of this.#webRtcTransports.values())
		// {
		// 	webRtcTransport.listenServerClosed();
	
		// 	// Emit observer event.
		// 	this.#observer.safeEmit('webrtctransportunhandled', webRtcTransport);
		// }
		// this.#webRtcTransports.clear();
	
		this.emit('@close');
	
		// // Emit observer event.
		// this.#observer.safeEmit('close');
	}
}