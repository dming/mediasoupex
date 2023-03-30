import { Logger } from '../Logger';
import { RtmpTransport, RtmpTransportEvents, RtmpTransportInternal, TransportConstructorOptions } from './RtmpTransport';

export type RtmpServerTransportInternal = RtmpTransportInternal &
{
};

export type RtmpServerTransportEvents = RtmpTransportEvents &
{ 
	
};

type RtmpServerTransportConstructorOptions = TransportConstructorOptions &
{
	data: RtmpServerTransportData;
};

export type RtmpServerTransportData =
{
};

const logger = new Logger('RtmpServerTransport');

export class RtmpServerTransport extends RtmpTransport<RtmpServerTransportEvents>
{

	constructor(options: RtmpServerTransportConstructorOptions)
	{
		super(options);

		logger.debug('constructor() ');
	}
}