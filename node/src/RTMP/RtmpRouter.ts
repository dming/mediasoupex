import { Logger } from '../Logger';
import { Channel } from '../Channel';
import { PayloadChannel } from '../PayloadChannel';
import { EnhancedEventEmitter } from '../EnhancedEventEmitter';
import { v4 as uuidv4 } from 'uuid';
import { RtmpDirectTransport } from './RtmpDirectTransport';
import { IRtmpConsumer, RtmpMsg } from './RtmpType';
import { FlvSession } from './FlvSession';

export type RtmpRouterInternal =
{
	rtmpRouterId: string;
	streamUrl:string;
};

export type RtmpRouterEvents =
{ 
	rtmpServerclose: [];
	// Private events.
	'@close': [];
};

const logger = new Logger('RtmpRouter');

export class RtmpRouter extends EnhancedEventEmitter<RtmpRouterEvents>
{

	// Internal data.
	readonly #internal: RtmpRouterInternal;

	// Channel instance.
	readonly #channel: Channel;
	readonly #payloadChannel: PayloadChannel;

	readonly #streamUrl: string;

	defaultDirectTransport?: RtmpDirectTransport;

	readonly #consumers : Set<IRtmpConsumer>= new Set();

	isFirstAudioReceived = false;
	isFirstVideoReceived = false;
	metaData :Buffer| undefined;
	aacSequenceHeader :Buffer | undefined;
	avcSequenceHeader :Buffer| undefined;
	audioCodec = 0;
	videoCodec = 0;
	flvGopCacheQueue: Array<Buffer> = [];

	constructor(
		{
			internal,
			channel,
			payloadChannel
		}:
		{
			internal : RtmpRouterInternal;
			channel: Channel;
			payloadChannel: PayloadChannel;
		}
	)
	{
		super();

		logger.debug('constructor() streamUrl=%s, id=%s', internal.streamUrl, internal.rtmpRouterId);
		this.#internal = internal;
		this.#streamUrl = internal.streamUrl;
		this.#channel = channel;
		this.#payloadChannel = payloadChannel;

		this.handleWorkerNotifications();

		// eslint-disable-next-line no-constant-condition
		if (true) // todo: 应该根据配置去设置是否创建DefaultDirectTransport
		{
			this.createDefaultDirectTransport();
		}
	}

	private handleWorkerNotifications(): void
	{
		logger.debug('handleWorkerNotifications rtmp router id = ', this.#internal.rtmpRouterId);
		// eslint-disable-next-line @typescript-eslint/no-unused-vars
		this.#channel.on(this.#internal.rtmpRouterId, (event: string, data?: any) =>
		{
			switch (event)
			{
				case 'create_transport':
				{
					logger.debug('handleWorkerNotifications on `create_transport` data is %j', data);
					break;
				}

				default:
				{
					logger.error('ignoring unknown event "%s"', event);
				}
			}
		});
	}

	private async createDefaultDirectTransport()
	{
		logger.debug('createDefaultDirectTransport');

		const reqData = {
			rtmpTransportId : uuidv4(),
			isPublish       : false
		};

		const recvData = await this.#channel.request('rtmpRouter.createDirectTransport', this.#internal.rtmpRouterId, reqData);

		logger.debug('createDefaultDirectTransport recvData=%j', recvData);
		
		this.defaultDirectTransport = new RtmpDirectTransport({
			internal : {
				rtmpTransportId : reqData.rtmpTransportId
			},
			channel        : this.#channel,
			payloadChannel : this.#payloadChannel,
			data           : {}
		});

		this.defaultDirectTransport.on('rtmpmsg', (msg: RtmpMsg) => 
		{
			this.onRtmpMsg(msg);
		});
	}

	private onRtmpMsg(msg: RtmpMsg) 
	{
		switch (msg.messageType) 
		{
			case 8:
				this.handleRtmpAudioMsg(msg);
				break;
			case 9:
				this.handleRtmpVideoMsg(msg);
				break;
			case 18:
				this.handleRtmpDataMsg(msg);
				break;
			default:
				logger.debug('msg type ', msg.messageType); 
				break;
		}

		for (const consumer of this.#consumers) 
		{
			consumer.enqueue(msg);
		}
	}

	private handleRtmpAudioMsg(msg: RtmpMsg) 
	{
		const payload = msg.payload;
		const soundFormat = (payload[0] >> 4) & 0x0f;
		// const soundType = payload[0] & 0x01;
		// const soundSize = (payload[0] >> 1) & 0x01;
		// const soundRate = (payload[0] >> 2) & 0x03;

		if (this.audioCodec == 0) 
		{
			this.audioCodec = soundFormat;
		}

		if ((soundFormat == 10 || soundFormat == 13) && payload[1] == 0) 
		{
			// cache aac sequence header
			this.isFirstAudioReceived = true;
			this.aacSequenceHeader = Buffer.alloc(payload.length);
			payload.copy(this.aacSequenceHeader);
		}

		const flvTag = FlvSession.createFlvTag(
			msg.messageType, msg.timestamp, msg.payload, payload.length);
		
		// cache gop
		if (this.aacSequenceHeader != null && payload[1] === 0) 
		{
			// skip aac sequence header
		}
		else
		{
			this.flvGopCacheQueue.push(flvTag);
		}
		
	}

	private handleRtmpVideoMsg(msg: RtmpMsg) 
	{
		const payload = msg.payload;
		const frameType = (payload[0] >> 4) & 0x0f;
		const codecId = payload[0] & 0x0f;

		if (codecId == 7 || codecId == 12) 
		{
			// cache avc sequence header
			if (frameType == 1 && payload[1] == 0) 
			{
				this.isFirstVideoReceived = true;
				this.avcSequenceHeader = Buffer.alloc(payload.length);
				payload.copy(this.avcSequenceHeader);
			}
		}

		if (this.videoCodec == 0) 
		{
			this.videoCodec = codecId;
		}

		const flvTag = FlvSession.createFlvTag(
			msg.messageType, msg.timestamp, msg.payload, msg.payload.length);

		// cache gop
		if (frameType == 1) 
		{
			this.flvGopCacheQueue.length = 0;
		}
		if ((codecId == 7 || codecId == 12) && frameType == 1 && payload[1] == 0) 
		{
			// skip avc sequence header
		}
		else 
		{
			this.flvGopCacheQueue.push(flvTag);
		}
	}
	
	private handleRtmpDataMsg(msg: RtmpMsg) 
	{
		this.metaData = msg.payload;

	}

	onCreateConsumer(consumer: IRtmpConsumer)
	{
		this.#consumers.add(consumer);
	}

	onRemoveConsumer(consumer: IRtmpConsumer) 
	{
		this.#consumers.delete(consumer);
	}

	/**
	 * 监听DefaultDirectTransport，获得信息并处理
	 * 提供创建FlvSession的接口，并且将前面获得的信息转发给FlvSession
	 */
}