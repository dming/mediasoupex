// todo: 获得HttpReq 和 HttpRes之后，创建FlvSession，然后作为player注册到RtmpRouter里，
//  RtmpRouter的DefaultDerectTransport收到的信息将会发送给所有player。

// http.IncomingMessage http.ServerResponse
import * as http from 'http';
import { RtmpRouter } from './RtmpRouter';
import { IRtmpConsumer, RtmpMsg } from './RtmpType';

export class FlvSession implements IRtmpConsumer
{
	static createFlvTag(
		messageType:number, timestamp: number, payload: Buffer, length:number) 
	{
		const PreviousTagSize = 11 + length;
		const tagBuffer = Buffer.alloc(PreviousTagSize + 4);

		tagBuffer[0] = messageType;
		tagBuffer.writeUIntBE(length, 1, 3);
		tagBuffer[4] = (timestamp >> 16) & 0xff;
		tagBuffer[5] = (timestamp >> 8) & 0xff;
		tagBuffer[6] = timestamp & 0xff;
		tagBuffer[7] = (timestamp >> 24) & 0xff;
		tagBuffer.writeUIntBE(0, 8, 3);
		tagBuffer.writeUInt32BE(PreviousTagSize, PreviousTagSize);
		payload.copy(tagBuffer, 11, 0, length);
		
		return tagBuffer;
	}

	#req: http.IncomingMessage;
	#res: http.ServerResponse;

	#rtmpRouter: RtmpRouter;

	private started = false;
    
	constructor(
		rtmpRouter: RtmpRouter,
		req: http.IncomingMessage,
		res: http.ServerResponse) 
	{
		this.#rtmpRouter = rtmpRouter;
		this.#req = req;
		this.#res = res;

	}

	onStart()
	{
		if (this.started) { return; }
		else 
		{
			this.started = true;
            
			// send FLV header
			const FLVHeader = Buffer.from(
				[ 0x46, 0x4c, 0x56, 0x01, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00 ]
			);

			if (this.#rtmpRouter.isFirstAudioReceived) 
			{
				FLVHeader[4] |= 0b00000100;
			}

			if (this.#rtmpRouter.isFirstVideoReceived) 
			{
				FLVHeader[4] |= 0b00000001;
			}
			this.#res.write(FLVHeader);

			// send Metadata
			if (this.#rtmpRouter.metaData) 
			{
				const tag = FlvSession.createFlvTag(
					18,
					0,
					this.#rtmpRouter.metaData,
					this.#rtmpRouter.metaData.length);

				this.#res.write(tag);
			}

			// send aacSequenceHeader
			if (this.#rtmpRouter.audioCodec == 10 && this.#rtmpRouter.aacSequenceHeader) 
			{
				const tag = FlvSession.createFlvTag(
					8,
					0,
					this.#rtmpRouter.aacSequenceHeader,
					this.#rtmpRouter.aacSequenceHeader.length);

				this.#res.write(tag);
			}

			// send avcSequenceHeader
			if (
				(this.#rtmpRouter.videoCodec == 7 || this.#rtmpRouter.videoCodec == 12) 
				&& this.#rtmpRouter.avcSequenceHeader
			) 
			{
				const tag = FlvSession.createFlvTag(
					9,
					0,
					this.#rtmpRouter.avcSequenceHeader,
					this.#rtmpRouter.avcSequenceHeader.length);

				this.#res.write(tag);
			}

			// send gop cache
			if (this.#rtmpRouter.flvGopCacheQueue) 
			{
				for (const tag of this.#rtmpRouter.flvGopCacheQueue) 
				{
					this.#res.write(tag);
				}
			}
		}
	}

	onStop() 
	{
		this.#res.end();
	}

	enqueue(msg: RtmpMsg)
	{
		const flvTag = FlvSession.createFlvTag(
			msg.messageType, msg.timestamp, msg.payload, msg.payload.length);

		this.#res.write(flvTag); 
	}
}