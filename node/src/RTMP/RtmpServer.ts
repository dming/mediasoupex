import { Logger } from '../Logger';
import { Channel } from '../Channel';
import { PayloadChannel } from '../PayloadChannel';
import { EnhancedEventEmitter } from '../EnhancedEventEmitter';
import { RtmpRouter, RtmpRouterInternal } from './RtmpRouter';

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

export type RtmpServerObserverEvents =
{
	close: [];
	// newtransport: [Transport];
	// newrtpobserver: [RtpObserver];
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
	
	// PayloadChannel instance.
	readonly #payloadChannel: PayloadChannel;

	// Closed flag.
	#closed = false;

	// Custom app data.
	readonly #appData: Record<string, unknown>;

	// Observer instance.
	readonly #observer = new EnhancedEventEmitter<RtmpServerObserverEvents>();

	readonly #routers : Map<string, RtmpRouter> = new Map;

	constructor(
		{
			internal,
			channel,
			payloadChannel,
			appData
		}:
		{
			internal: RtmpServerInternal;
			channel: Channel;
			payloadChannel: PayloadChannel;
			appData?: Record<string, unknown>;
		}
	)
	{
		super();
		logger.debug('constructor()');

		this.#internal = internal;
		this.#channel = channel;
		this.#payloadChannel = payloadChannel;
		this.#appData = appData || {};

		this.handleWorkerNotifications();
	}

	/**
	 * App custom data.
	 */
	get appData(): Record<string, unknown>
	{
		return this.#appData;
	}

	/**
	 * Invalid setter.
	 */
	set appData(appData: Record<string, unknown>) // eslint-disable-line no-unused-vars
	{
		throw new Error('cannot override appData object');
	}

	/**
	 * Observer.
	 */
	get observer(): EnhancedEventEmitter<RtmpServerObserverEvents>
	{
		return this.#observer;
	}
	
	private handleWorkerNotifications(): void
	{
		// eslint-disable-next-line @typescript-eslint/no-unused-vars
		this.#channel.on(this.#internal.rtmpServerId, (event: string, data?: any) =>
		{
			switch (event) 
			{
				case 'create_rtmp_router':
				{
					if (data && Object.prototype.toString.call(data) == '[object Object]')
					{
						if (data.id && data.stream_url)
						{
							let streamUrl: string = data.stream_url;

							streamUrl = streamUrl.substring(streamUrl.indexOf('/'));
							const internal = {
								streamUrl    : streamUrl,
								rtmpRouterId : data.id
							} as RtmpRouterInternal;
							const router = new RtmpRouter({ 
								internal, 
								channel        : this.#channel, 
								payloadChannel : this.#payloadChannel
							});
							
							this.#routers.set(internal.streamUrl, router); 
							logger.debug('`create_rtmp_router` streamUrl is %s', streamUrl);
						}
					}
					break;
				}

				default:
				{
					logger.error('ignoring unknown event "%s"', event);
				}
			}
		});
	}

	public getRouter(streamUrl: string) : RtmpRouter | undefined
	{
		return this.#routers.get(streamUrl);
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
		this.#observer.safeEmit('close');
	}

	/**
	 * Worker was closed.
	 *
	 * @private
	 */
	workerClosed(): void
	{
		if (this.#closed)
			return;

		logger.debug('workerClosed()');

		this.#closed = true;

		this.safeEmit('workerclose');

		// Emit observer event.
		this.#observer.safeEmit('close');
	}
}