import { Logger } from '../Logger';
import { RtmpTransport, RtmpTransportEvents, RtmpTransportInternal, TransportConstructorOptions } from './RtmpTransport';
import { RtmpMsg } from './RtmpType';

export type RtmpDirectTransportInternal = RtmpTransportInternal &
{
};

export type RtmpDirectTransportEvents = RtmpTransportEvents &
{ 
	rtmpmsg: [RtmpMsg];
};

type RtmpDirectTransportConstructorOptions = TransportConstructorOptions &
{
	data: RtmpDirectTransportData;
};

export type RtmpDirectTransportData =
{
};

const logger = new Logger('RtmpDirectTransport');

export class RtmpDirectTransport extends RtmpTransport<RtmpDirectTransportEvents>
{

	constructor(options: RtmpDirectTransportConstructorOptions)
	{
		super(options);

		logger.debug('constructor() id = ', options.internal.rtmpTransportId);

		this.handleWorkerNotifications();
	}

	private handleWorkerNotifications(): void
	{
		// eslint-disable-next-line @typescript-eslint/no-unused-vars
		this.channel.on(this.internal.rtmpTransportId, (event: string, data?: any) =>
		{
			switch (event)
			{
				default:
				{
					logger.error('ignoring unknown event "%s"', event);
				}
			}
		});

		this.payloadChannel.on(
			this.internal.rtmpTransportId,
			(event: string, data: any | undefined, payload: Buffer) =>
			{
				switch (event)
				{
					case 'rtmpmsg':
					{
						if (this.closed)
							break;

						// logger.debug('payload channel recive data=%j', data);

						const msg: RtmpMsg = {
							messageType : data.message_type,
							timestamp    : data.timestamp,
							size         : data.size,
							payload
						};

						this.safeEmit('rtmpmsg', msg);

						break;
					}

					default:
					{
						logger.error('ignoring unknown event "%s"', event);
					}
				}
			});
	}
}