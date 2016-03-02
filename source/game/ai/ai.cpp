#include "bot.h"

bool Ai::NodeReachedPlatformEnd()
{
    bool reached = false;

    if( self->ai->next_node == NODE_INVALID )
        return true;

    if( self->groundentity && self->groundentity->use == Use_Plat )
    {
        reached = ( self->groundentity->moveinfo.state == STATE_TOP
                    || VectorCompare( self->groundentity->s.origin, self->groundentity->moveinfo.dest ) )
                  ? true : false;
    }
    else
    {
        vec3_t v1, v2;

        v1[0] = self->s.origin[0];
        v1[1] = self->s.origin[1];
        v1[2] = 0;

        v2[0] = nodes[self->ai->next_node].origin[0];
        v2[1] = nodes[self->ai->next_node].origin[1];
        v2[2] = 0;

        if( DistanceFast( v1, v2 ) < NODE_REACH_RADIUS )
            reached =
                    ( fabs( nodes[self->ai->next_node].origin[2] - self->s.origin[2] ) < ( AI_JUMPABLE_HEIGHT * 0.5 ) )
                    ? true : false;
    }

    return reached;
}

bool Ai::NodeReachedPlatformStart()
{
    bool reached = false;

    if( self->ai->next_node == NODE_INVALID )
        return true;

    if( self->groundentity && self->groundentity->use == Use_Plat )
    {
        vec3_t v1, v2;

        v1[0] = self->s.origin[0];
        v1[1] = self->s.origin[1];
        v1[2] = 0;

        v2[0] = nodes[self->ai->next_node].origin[0];
        v2[1] = nodes[self->ai->next_node].origin[1];
        v2[2] = 0;

        reached = ( DistanceFast( v1, v2 ) < NODE_REACH_RADIUS ) ? true : false;
    }

    return reached;
}

bool Ai::ReachabilityVisible(vec3_t point) const
{
    trace_t trace;

    G_Trace( &trace, self->s.origin, vec3_origin, vec3_origin, point, self, MASK_DEADSOLID );
    if( trace.ent < 0 )
        return true;

    return false;
}

bool Ai::NodeReachedGeneric()
{
    bool reached = false;
    float RADIUS = NODE_REACH_RADIUS;

    if( !( GetNodeFlags( self->ai->next_node ) & (NODEFLAGS_REACHATTOUCH|NODEFLAGS_ENTITYREACH) ) )
    {
        if( self->ai->path.numNodes >= MIN_BUNNY_NODES )
        {
            int n1 = self->ai->path.nodes[self->ai->path.numNodes];
            int n2 = self->ai->path.nodes[self->ai->path.numNodes-1];
            vec3_t n1origin, n2origin, origin;

            // if falling from a jump pad use a taller cylinder
            if( !self->groundentity && !self->is_step && !self->is_swim
                && ( CurrentLinkType() & LINK_JUMPPAD ) )
                RADIUS = NODE_WIDE_REACH_RADIUS;

            // we use a wider radius in 2D, and a height range enough so they can't be jumped over
            GetNodeOrigin( n1, n1origin );
            GetNodeOrigin( n2, n2origin );
            VectorCopy( self->s.origin, origin );
            n1origin[2] = n2origin[2] = origin[2] = 0;

            // see if reached the second
            if( n2 != NODE_INVALID &&
                ( ( nodes[n2].origin[2] - 16 ) < self->s.origin[2] ) &&
                ( nodes[n2].origin[2] + RADIUS > self->s.origin[2] ) &&
                ( DistanceFast( n2origin, origin ) < RADIUS )
                    )
            {
                NodeReached(); // advance the first
                reached = true;		// return the second as reached
            }
                // see if reached the first
            else if( ( ( nodes[n1].origin[2] - 16 ) < self->s.origin[2] ) &&
                     ( nodes[n1].origin[2] + RADIUS > self->s.origin[2] ) &&
                     ( DistanceFast( n1origin, origin ) < RADIUS ) )
            {
                reached = true; // return the first as reached
            }
        }
        else
        {
            reached = ( DistanceFast( self->s.origin, nodes[self->ai->next_node].origin ) < RADIUS ) ? true : false;
        }
    }

    return reached;
}

bool Ai::NodeReachedSpecial()
{
    bool reached = false;

    if( self->ai->next_node != NODE_INVALID && !( GetNodeFlags( self->ai->next_node ) & (NODEFLAGS_REACHATTOUCH|NODEFLAGS_ENTITYREACH) ) )
    {
        if( self->ai->path.numNodes >= MIN_BUNNY_NODES )
        {
            int n1 = self->ai->path.nodes[self->ai->path.numNodes];
            int n2 = self->ai->path.nodes[self->ai->path.numNodes-1];
            vec3_t n1origin, n2origin, origin;

            // we use a wider radius in 2D, and a height range enough so they can't be jumped over
            GetNodeOrigin( n1, n1origin );
            GetNodeOrigin( n2, n2origin );
            VectorCopy( self->s.origin, origin );
            n1origin[2] = n2origin[2] = origin[2] = 0;

            // see if reached the second
            if( ( ( nodes[n2].origin[2] - 16 ) < self->s.origin[2] ) &&
                ( nodes[n2].origin[2] + NODE_WIDE_REACH_RADIUS > self->s.origin[2] ) &&
                ( DistanceFast( n2origin, origin ) < NODE_WIDE_REACH_RADIUS ) &&
                ReachabilityVisible( nodes[n2].origin ) )
            {
                NodeReached(); // advance the first
                reached = true;		// return the second as reached
            }
                // see if reached the first
            else if( ( ( nodes[n1].origin[2] - 16 ) < self->s.origin[2] ) &&
                     ( nodes[n1].origin[2] + NODE_WIDE_REACH_RADIUS > self->s.origin[2] ) &&
                     ( DistanceFast( n1origin, origin ) < NODE_WIDE_REACH_RADIUS ) &&
                     ReachabilityVisible(nodes[n1].origin ) )
            {
                reached = true; // return the first as reached
            }
        }
        else
            return NodeReachedGeneric();
    }

    return reached;
}

bool Ai::AttemptWalljump()
{
    if( self->ai->path.numNodes >= 1 )
    {
        int n1 = self->ai->current_node;
        int n2 = self->ai->next_node;
        vec3_t n1origin, n2origin, origin;

        if( n1 == n2 )
            return false;

        // we use a wider radius in 2D, and a height range enough so they can't be jumped over
        GetNodeOrigin( n1, n1origin );
        GetNodeOrigin( n2, n2origin );
        VectorCopy( self->s.origin, origin );

        if( fabs( n1origin[2] - n2origin[2] ) < 32.0f && origin[2] >= n1origin[2] - 4.0f ) {
            float dist = DistanceFast( n1origin, n2origin );
            float n1d, n2d;

            n1d = DistanceFast( n1origin, origin );
            n2d = DistanceFast( n2origin, origin );

            if( dist >= 150.0f &&
                n1d >= dist*0.5f &&
                n2d < dist ) {
                return true;
            }
        }
    }

    return false;
}
